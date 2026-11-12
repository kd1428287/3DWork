#pragma once

#include <atomic>
#include <mutex>
#include <variant>
#include <unordered_map>

#include "Application/Core/EventBus/Events/ShapeEffectEvents.h"
#include "ShapeEffectInstance.h"
#include "../Common/KdAssetsTextureProvider.h"

// エフェクト名に対応する定義(見た目+動き)
struct ShapeEffectDefinition
{
	ShapeEffectParams				Params;
	std::shared_ptr<IEffectMotion>	Motion;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 一枚テクスチャエフェクトのディスパッチャー
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// シーン単位のEventBusを購読し、ShapeEffectAttach〜BaseUpdate〜Detachでインスタンスを管理する。
// EffectDispatcherと同じく、イベントハンドラはpendingへ積むだけで、実処理はUpdate()冒頭で行う
// (Publish()は任意のスレッドから呼んでよい。Update()/Draw()は描画スレッドから呼ぶ)。
//
// 【使い方】
//   dispatcher.Init(bus);
//   dispatcher.RegisterDefinition("Name", def);
//   Events::PublishShapeEffectAttach(bus, "key", "Name", baseMatrix);
//   Events::PublishShapeEffectBaseUpdate(bus, "key", baseMatrix);   // 追従させる間、毎フレーム
//   dispatcher.Update(dt);  /  dispatcher.Draw(ParticleDrawPass::Default);
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class ShapeEffectDispatcher
{
public:

	ShapeEffectDispatcher() {}
	~ShapeEffectDispatcher() { Release(); }

	ShapeEffectDispatcher(const ShapeEffectDispatcher&) = delete;
	ShapeEffectDispatcher& operator=(const ShapeEffectDispatcher&) = delete;

	// dataPathのJSONから定義を読み込む(無い・壊れている場合も、コードからのRegisterDefinitionだけで動ける)
	bool Init(EventBus& bus, const std::string& dataPath = "Asset/Data/Game/shapeeffectmap.json");
	void Release();

	// Attachで使う名前の定義を登録する(同名は上書き)
	void RegisterDefinition(const std::string& name, const ShapeEffectDefinition& def);

	// pathと同じJSONを読み込んでいる全ShapeEffectDispatcherへ、再ロード要求を立てる(実処理は次回Update()冒頭)
	static void NotifyDataSaved(const std::string& path);

	// pendingの消化 → 全インスタンスの更新 → 終了したものの破棄
	void Update(float deltaTime);

	// params.DrawPassがpassと一致するものだけ描画する
	void Draw(ParticleDrawPass pass);

private:

	struct PendingAttach
	{
		std::string		InstanceKey;
		std::string		EffectName;
		Math::Matrix	Base;
		float			MaxDuration = 0.0f;
	};
	struct PendingBaseUpdate
	{
		std::string		InstanceKey;
		Math::Matrix	Base;
	};
	struct PendingDetach
	{
		std::string		InstanceKey;
	};
	using PendingEvent = std::variant<PendingAttach, PendingBaseUpdate, PendingDetach>;

	void ProcessPendingEvents();

	// JSONの定義を読み込んで登録する(同名は上書き。JSONから消えた定義は残る)
	bool LoadData(const std::string& path);

	struct Active
	{
		ShapeEffectInstance	instance;
		Math::Matrix		base;
		float				maxDurationRemaining = -1.0f;	// 0以上なら残り秒数で破棄(負なら無期限)
	};

	std::unordered_map<std::string, ShapeEffectDefinition>	definitions_;
	std::unordered_map<std::string, Active>					active_;

	KdAssetsTextureProvider			textureProvider_;
	std::vector<ScopedSubscriber>	subscriptions_;

	std::mutex					pendingMutex_;
	std::vector<PendingEvent>	pendingEvents_;

	std::string					dataPath_;
	std::atomic<bool>			reloadRequested_ = false;

	// 生存中の全インスタンス(NotifyDataSaved()用)
	static std::vector<ShapeEffectDispatcher*>	s_instances;
	static std::mutex							s_instancesMutex;
};
