#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "Application/Core/EventBus/Events/EffectEvents.h"	// EventBus / ScopedSubscriber
#include "Application/Core/EventBus/Events/WindShellEvents.h"
#include "Framework/Shader/GPUParticle/ParticleBuffer.h"
#include "WindShellInstance.h"
#include "Application\Definitions\Effect\EffectParams.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 風シェル(身にまとう風)のディスパッチャー
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// EffectDispatcherと同じ方式：イベントはpendingに積むだけで、実処理はUpdate()冒頭に一本化する。
// Publishは任意スレッドから可。Update()/Draw()は描画スレッドから呼ぶ。描画はDefaultパスのみ。
//
// 【使い方】
//   dispatcher.Init(*localBus_);
//   dispatcher.RegisterPreset("EnemyAura", WindShellPreset::EnemyAura());
//   dispatcher.Update(dt);                              // OnPreDraw内
//   dispatcher.Draw(ParticleDrawPass::Default);         // OnDrawEffects内(トレイルの後)
//
//   WindShellHandle wind;                               // 呼び出し側(コンポーネント)
//   wind.Start(bus, "EnemyAura", pos, axis);
//   wind.Update(pos, axis, progress);                   // 毎フレーム
//   wind.Stop();                                        // 消去へ(自然に消える)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class WindShellDispatcher
{
public:

	WindShellDispatcher() {}
	~WindShellDispatcher() { Release(); }

	WindShellDispatcher(const WindShellDispatcher&) = delete;
	WindShellDispatcher& operator=(const WindShellDispatcher&) = delete;

	bool Init(EventBus& bus);

	void Release();

	// 同名なら上書き(既に動いているインスタンスは影響を受けない)
	void RegisterPreset(const std::string& name, const WindShellPreset& preset);

	// pendingの消化 → 全インスタンスの更新 → 消え切ったものの破棄
	void Update(float deltaTime);

	void Draw(ParticleDrawPass pass);

private:

	struct PendingAttach { std::string InstanceKey; std::string PresetName; Math::Vector3 Position; Math::Vector3 Axis; };
	struct PendingUpdate { std::string InstanceKey; Math::Vector3 Position; Math::Vector3 Axis; float Progress = 0.0f; };
	struct PendingStop   { std::string InstanceKey; };
	struct PendingDetach { std::string InstanceKey; };
	using PendingEvent = std::variant<PendingAttach, PendingUpdate, PendingStop, PendingDetach>;

	// イベントハンドラ：pendingEvents_へ積むだけ(mutexで保護)
	void OnAttach(const Events::WindShellAttach& e);
	void OnUpdate(const Events::WindShellUpdate& e);
	void OnStop(const Events::WindShellStop& e);
	void OnDetach(const Events::WindShellDetach& e);

	void ProcessPendingEvents();

	std::unordered_map<std::string, WindShellPreset>	presets_;
	std::unordered_map<std::string, WindShellInstance>	active_;

	std::vector<ScopedSubscriber>	subscriptions_;

	std::mutex					pendingMutex_;
	std::vector<PendingEvent>	pendingEvents_;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 呼び出し側が持つ操作用ハンドル(キーの生成とイベント発行をまとめただけの薄いクラス)
// ※デストラクタでは何も発行しない(バスが先に破棄されている場合があるため)。終了時はStop()を呼ぶ事
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class WindShellHandle
{
public:

	WindShellHandle() = default;

	WindShellHandle(const WindShellHandle&) = delete;
	WindShellHandle& operator=(const WindShellHandle&) = delete;

	WindShellHandle(WindShellHandle&& o) noexcept : bus_(o.bus_), key_(std::move(o.key_)) { o.bus_ = nullptr; }
	WindShellHandle& operator=(WindShellHandle&& o) noexcept
	{
		if (this != &o) { bus_ = o.bus_; key_ = std::move(o.key_); o.bus_ = nullptr; }
		return *this;
	}

	bool IsActive() const { return bus_ != nullptr; }

	// 既に動いていれば置き換える
	void Start(EventBus& bus, const std::string& presetName, const Math::Vector3& pos, const Math::Vector3& axis)
	{
		static std::atomic<uint32_t> s_counter{ 0 };

		if (IsActive()) { Detach(); }

		bus_ = &bus;
		key_ = "__WindShell_" + std::to_string(s_counter.fetch_add(1));

		Events::WindShellAttach e;
		e.InstanceKey = key_;
		e.PresetName = presetName;
		e.Position = pos;
		e.Axis = axis;
		bus_->Publish(e);
	}

	void Update(const Math::Vector3& pos, const Math::Vector3& axis, float progress)
	{
		if (!IsActive()) { return; }

		Events::WindShellUpdate e;
		e.InstanceKey = key_;
		e.Position = pos;
		e.Axis = axis;
		e.Progress = progress;
		bus_->Publish(e);
	}

	// 消去へ移す(以後このハンドルは無効)
	void Stop()
	{
		if (!IsActive()) { return; }

		Events::WindShellStop e;
		e.InstanceKey = key_;
		bus_->Publish(e);
		bus_ = nullptr;
	}

	// 即時破棄(以後このハンドルは無効)
	void Detach()
	{
		if (!IsActive()) { return; }

		Events::WindShellDetach e;
		e.InstanceKey = key_;
		bus_->Publish(e);
		bus_ = nullptr;
	}

private:

	EventBus*	bus_ = nullptr;
	std::string	key_;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// イベント発行ヘルパー(他のエフェクトのPublishXxxと同じ呼び方)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
namespace Events
{
	inline void PublishWindShellAttach(EventBus& bus, const std::string& key, const std::string& preset,
		const Math::Vector3& pos, const Math::Vector3& axis)
	{
		WindShellAttach e;
		e.InstanceKey = key;
		e.PresetName = preset;
		e.Position = pos;
		e.Axis = axis;
		bus.Publish(e);
	}

	inline void PublishWindShellUpdate(EventBus& bus, const std::string& key,
		const Math::Vector3& pos, const Math::Vector3& axis, float progress)
	{
		WindShellUpdate e;
		e.InstanceKey = key;
		e.Position = pos;
		e.Axis = axis;
		e.Progress = progress;
		bus.Publish(e);
	}

	inline void PublishWindShellStop(EventBus& bus, const std::string& key)
	{
		WindShellStop e;
		e.InstanceKey = key;
		bus.Publish(e);
	}

	inline void PublishWindShellDetach(EventBus& bus, const std::string& key)
	{
		WindShellDetach e;
		e.InstanceKey = key;
		bus.Publish(e);
	}
}
