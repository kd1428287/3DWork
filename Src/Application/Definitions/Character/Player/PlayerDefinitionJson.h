#pragma once

#include "../../Loaders/DefinitionJson.h"
#include "Application/Definitions/Character/Player/PlayerCombatBehaviorDefinition.h"

// Player専用型のJSON変換(共有型はDefinitionJson.h)。
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
