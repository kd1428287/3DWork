#pragma once
#include "Application/Definitions/Character/Common/FootstepTypes.h"
#include "../../../Graphics/Animation/ModelAnimatorComponent.h"

// ============================================================
// FootstepEventComponent
// 配置場所: Application/Components/Graphics/Animation/
//
// ModelAnimatorComponent::GetNormalizedTime()を毎フレーム監視し、
// クリップごとに登録された接地phase(FootstepTrigger)を通過した瞬間に
// FootstepEventをEventBusへPublishする。
//
// 【責務】
// 「どのクリップの、どのphaseで足がつくか」というデータ(triggerTable_)と
// 検知ロジックだけを持つ。パーティクル生成やボーン位置の解決は
// FootDustComponent等の購読側に任せる
// (RootMotionExtractor/RootMotionApplierComponentと同じ分離方針)。
//
// 【登録方法】
// ComponentRegistrations.cppのBuildFootDust()から、Player.json側の
// "clips"パラメータを元にRegisterClip(clipName, triggers)を呼んで登録する。
//
// 【クリップ切り替え時の扱い】
// クリップ名が前フレームと変わった直後は、直前クリップのphaseとの比較が
// 無意味なため現在位置に合わせるだけで判定をスキップする(誤検知防止)。
// ============================================================
class FootstepEventComponent : public ComponentBase
{
public:
	explicit FootstepEventComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override
	{
		animator_ = GetOwner()->GetComponent<ModelAnimatorComponent>();
	}

	// クリップ名ごとの接地phaseリストを登録する(同名クリップは上書き)。
	void RegisterClip(const std::string& clipName, std::vector<FootstepTrigger> triggers)
	{
		triggerTable_[clipName] = std::move(triggers);
	}

	void Update(float deltaTime) override;

private:
	static bool CrossedPhase(float lastPhase, float currentPhase, float triggerPhase);
	const std::vector<FootstepTrigger>* FindTriggersForClip(std::string_view clipName) const;

	ModelAnimatorComponent* animator_ = nullptr;

	std::unordered_map<std::string, std::vector<FootstepTrigger>> triggerTable_;

	std::string lastClipName_;
	float lastPhase_ = 0.0f;
	bool hasLastClip_ = false;
};