#pragma once

#include "../../Loaders/DefinitionJson.h"
#include "Application/Definitions/Loaders/ComponentParamsOrderedJson.h"
#include "Application/Definitions/Character/Player/PlayerCombatBehaviorDefinition.h"

// Player専用型のJSON変換(共有型はDefinitionJson.h)。
// COMPONENT_PARAMS_DEFINE_TYPEの説明・理由はComponentParamsOrderedJson.hのコメントを参照。
COMPONENT_PARAMS_DEFINE_TYPE(EvadeData,
	activeDuration, recoveryDuration, justWindowStart, justWindowEnd, evadeDistance, useRootMotion,
	animationNameForward, animationNameBackward, animationNameLeft, animationNameRight)

	COMPONENT_PARAMS_DEFINE_TYPE(GuardData,
		justWindowDuration, start, loop, parry, hit, end)

	COMPONENT_PARAMS_DEFINE_TYPE(MovementPhaseClips,
		startAnimationName, startDuration, loopAnimationName, endAnimationName, endDuration)

	COMPONENT_PARAMS_DEFINE_TYPE(WalkLockedAnimationSet,
		forward, forwardRight, right, backwardRight, backward, backwardLeft, left, forwardLeft)

	COMPONENT_PARAMS_DEFINE_TYPE(WalkAnimationSet, forward, locked)

	COMPONENT_PARAMS_DEFINE_TYPE(TurnAnimationSet, left90, right90, left180, right180)

	COMPONENT_PARAMS_DEFINE_TYPE(PlayerMovementAnimationDefinition, walk, turn, run, idle)