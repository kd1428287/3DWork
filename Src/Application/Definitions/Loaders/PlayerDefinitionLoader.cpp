#include "PlayerDefinitionLoader.h"

#include <nlohmann/json.hpp>
#include "JsonLoader.h"
#include "../Character/Common/CharacterDefinitionCommon.h"
#include "PlayerAttackTableLoader.h"
#include "DefinitionJson.h"

// Player専用型のJSON変換(共有型はDefinitionJson.h)。未知の軸名は先頭のYになる。
NLOHMANN_JSON_SERIALIZE_ENUM(RootMotionAxis, {
	{ RootMotionAxis::Y, "Y" },
	{ RootMotionAxis::X, "X" },
	{ RootMotionAxis::Z, "Z" },
})

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PlayerVisualDefinition,
	modelPath, animatorFPS, animatorScale, rootMotionBoneName,
	rootMotionAxis, rootMotionAxisSign, rootMotionScale)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PlayerCombatStatsDefinition, maxHealth)

// evadeDirectionは入力方向から毎回計算する値なので含めない。
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvadeData,
	activeDuration, recoveryDuration, justWindowStart, justWindowEnd, evadeDistance, useRootMotion,
	animationNameForward, animationNameBackward, animationNameLeft, animationNameRight)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GuardData,
	justWindowDuration, animationName, startDuration, loopAnimationName,
	parrySuccessAnimationName, parrySuccessDuration, guardHitAnimationName, guardHitDuration)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MovementPhaseClips,
	startAnimationName, startDuration, loopAnimationName, endAnimationName, endDuration)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(WalkLockedAnimationSet,
	forward, forwardRight, right, backwardRight, backward, backwardLeft, left, forwardLeft)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(WalkAnimationSet, forward, locked)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TurnAnimationSet, left90, right90, left180, right180)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PlayerMovementAnimationDefinition, walk, turn, run)

// combatBehaviorはコンボ木を別ファイルから読むため含めず、LoadFromFile内で個別に読む。
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PlayerDefinition,
	visuals, combatStats, components, movementAnimations, colliders, walkSpeed, runSpeed,
	auxiliarySocketBones, weaponSocketBone, rightArmIK, weapon)

namespace
{
	using json = nlohmann::json;

	// コンボ木はattackTablePathの専用ファイルから読み、読み込みに失敗した場合はoutの既存値を残す。
	void ReadCombatBehavior(const json& j, PlayerCombatBehaviorDefinition& out)
	{
		if (j.contains("attackTablePath")) {
			const std::string tablePath = j["attackTablePath"].get<std::string>();
			if (!PlayerAttackTableLoader::LoadFromFile(tablePath, out.attackTable)) {
				OutputDebugStringA(("PlayerDefinitionLoader: attack table load failed: " + tablePath + "\n").c_str());
			}
		}
		if (j.contains("evade")) out.evade = j["evade"].get<EvadeData>();
		if (j.contains("guard")) out.guard = j["guard"].get<GuardData>();
	}
}

bool PlayerDefinitionLoader::LoadFromFile(const std::string& path, PlayerDefinition& outDefinition)
{
	json root;
	if (!JsonLoader::Load(path, root)) return false;

	// キー欠落・型不一致・未知の名前は例外になるため、ここでまとめて捕捉する。
	try {
		PlayerDefinition def = root.get<PlayerDefinition>();
		if (root.contains("combatBehavior")) ReadCombatBehavior(root["combatBehavior"], def.combatBehavior);

		outDefinition = std::move(def);
		return true;
	}
	catch (const std::exception& e) {
		OutputDebugStringA(("PlayerDefinitionLoader: " + path + ": " + e.what() + "\n").c_str());
		return false;
	}
}
