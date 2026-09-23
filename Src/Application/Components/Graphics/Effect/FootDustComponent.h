#pragma once
#include "Application/Definitions/Character/Common/FootstepTypes.h"
#include "../../Core/BoneSocketComponent.h"
#include "Application/Core/EventBus/Events/EffectEvents.h" 

// ============================================================
// FootDustComponent
// 配置場所: Application/Components/GamePlay/Character/Common/
//
// FootstepEventComponentが発行するFootstepEventを購読し、
// 対応する足のBoneSocketComponent位置でGenericEffectSpawnEventを
// 発行する。実際のパーティクル生成(JSON定義の解決・Emit)は
// EffectDispatcher側の責務であり、このコンポーネントは
// 「どこで発生させるか」の翻訳だけを行う。
//
// 【組み立て方法】
// ComponentRegistrations.cppのBuildFootDust()から、
// SetFootSockets()/SetEventBus()/SetEffectId()で注入される。
// ============================================================
class FootDustComponent : public ComponentBase
{
public:
	explicit FootDustComponent(GameObject* owner) : ComponentBase(owner) {}

	// 左右の足ボーンに対応するBoneSocketComponentを注入する。
	void SetFootSockets(Handle<BoneSocketComponent>& leftFoot, Handle<BoneSocketComponent>& rightFoot)
	{
		footSocketLeft_ = leftFoot;
		footSocketRight_ = rightFoot;
	}

	// JSON上のエフェクト名(m_simpleEffectsのキー)。既定値は"FootDust"。
	void SetEffectId(std::string effectId) { effectId_ = std::move(effectId); }

	void Awake() override;

private:
	void OnFootstep(const FootstepEvent& event);

	Handle<BoneSocketComponent> footSocketLeft_{};
	Handle<BoneSocketComponent> footSocketRight_{};
	std::string effectId_ = "FootDust";

	ScopedSubscriber onFootSub_;
};