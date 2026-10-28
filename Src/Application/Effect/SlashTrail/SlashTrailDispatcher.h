#pragma once

#include <mutex>
#include <variant>

#include "Application/Core/EventBus/Events/SlashTrailEvents.h"
#include "SlashTrailInstance.h"
#include "../Common/KdAssetsTextureProvider.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル(斬撃の軌跡)生成ディスパッチャー
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// シーン単位のEventBusを購読し、SlashTrailBegin/PositionUpdate/Endを受けて
// SlashTrailInstanceの記録開始/位置更新/記録終了を行う(EffectDispatcherと対になる面エフェクト側)。
//
// 【イベント処理のタイミング】
//	OnXxx(購読コールバック)はpendingEvents_(mutex保護)に積むだけで、
//	activeTrails_/fadingTrails_の変更は全てUpdate()冒頭のProcessPendingEvents()で行う。
//	Publish()は任意のスレッドから呼べるが、Update()/Draw()は同一スレッドから呼ぶこと。
//
// 【記録中とフェードアウト中】
//	activeTrails_：InstanceKeyごとの記録中のトレイル。
//	fadingTrails_：記録終了後、消え切る(IsFinished)まで描画するトレイル。
//	Endで記録中→フェードアウト中へ移し、消え切ったらUpdate()が破棄する。
//
// 【使い方】
//   dispatcher.Init(sceneLocalEventBus);
//   dispatcher.RegisterDefinition("PlayerSwordTrail", params);
//   dispatcher.Update(deltaTime);                  // 毎フレーム
//   dispatcher.Draw(ParticleDrawPass::Default);    // DrawLit内から
//   dispatcher.Draw(ParticleDrawPass::Bright);     // DrawBright内から
//   PublishSlashTrailBegin(bus, "Sword_Player", "PlayerSwordTrail");
//   PublishSlashTrailPositionUpdate(bus, "Sword_Player", tipPos, basePos);
//   PublishSlashTrailEnd(bus, "Sword_Player");
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class SlashTrailDispatcher
{
public:

	SlashTrailDispatcher() {}
	~SlashTrailDispatcher() { Release(); }

	// コピー禁止(SlashTrailInstanceがコピー禁止の為)
	SlashTrailDispatcher(const SlashTrailDispatcher&) = delete;
	SlashTrailDispatcher& operator=(const SlashTrailDispatcher&) = delete;

	// busはシーンのライフサイクルに合わせて破棄されるバスを渡す想定
	bool Init(EventBus& bus);

	void Release();

	// トレイル定義を登録する(同名なら上書き)。Update()と同じスレッドから呼ぶこと
	void RegisterDefinition(const std::string& name, const SlashTrailParams& params);

	// pending処理→全トレイルの更新→消え切ったものの破棄
	void Update(float deltaTime);

	// DrawPassFlagsにpassが含まれるトレイルだけを描画する
	void Draw(ParticleDrawPass pass);

private:

	// 発生リクエスト(イベントハンドラはこれを積むだけ)
	struct PendingBegin
	{
		std::string	InstanceKey;
		std::string	TrailName;
	};
	struct PendingPositionUpdate
	{
		std::string						InstanceKey;
		DirectX::SimpleMath::Vector3	Tip;
		DirectX::SimpleMath::Vector3	Base;
	};
	struct PendingEnd
	{
		std::string	InstanceKey;
	};
	using PendingEvent = std::variant<PendingBegin, PendingPositionUpdate, PendingEnd>;

	void OnSlashTrailBegin(const Events::SlashTrailBegin& e);
	void OnSlashTrailPositionUpdate(const Events::SlashTrailPositionUpdate& e);
	void OnSlashTrailEnd(const Events::SlashTrailEnd& e);

	// pendingEvents_を取り出して、積まれた順に1件ずつ処理する
	void ProcessPendingEvents();

	void Apply(const PendingBegin& p);
	void Apply(const PendingPositionUpdate& p);
	void Apply(const PendingEnd& p);

	// keyの記録中トレイルを終了させ、フェードアウト側へ移す(無ければ何もしない)
	void FinishRecording(const std::string& key);

	// name → 定義(テンプレート)
	std::unordered_map<std::string, SlashTrailParams> trailDefinitions_;

	std::unordered_map<std::string, SlashTrailInstance> activeTrails_;
	std::vector<SlashTrailInstance> fadingTrails_;

	// パラメータのTexturePathからテクスチャを解決する(EffectDispatcherと同じ)
	KdAssetsTextureProvider textureProvider_;

	// 購読の自動解除用(Release時にまとめて解除される)
	std::vector<ScopedSubscriber> subscriptions_;

	EventBus* bus_ = nullptr;

	// イベントハンドラ(任意スレッド)とUpdate()(所有スレッド)の間の受け渡し用
	std::mutex					pendingMutex_;
	std::vector<PendingEvent>	pendingEvents_;
};
