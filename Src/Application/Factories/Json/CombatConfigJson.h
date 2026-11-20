#pragma once

#include "Application/Components/Graphics/Animation/BoneOffsetComponent.h"
#include "Application/Components/GamePlay/Character/Common/HealthComponent.h"
#include "Application/Components/GamePlay/Character/Common/LifeComponent.h"
#include "Application/Components/GamePlay/Character/Common/PostureComponent.h"

#include "Application/Definitions/Json/KdJson.h"

// 戦闘(共通)ComponentのConfigのJSON変換。
COMPONENT_PARAMS_DEFINE_TYPE(PostureConfig, max, lowerLimit, regenPerSecond, regenDelaySeconds)
COMPONENT_PARAMS_DEFINE_TYPE(HealthConfig, max)
COMPONENT_PARAMS_DEFINE_TYPE(LifeConfig, max)
COMPONENT_PARAMS_DEFINE_TYPE(BoneOffsetChainConfig, name, bones, weights, impulse, stiffness, damping, maxAngle,
	blendInSeconds, holdSeconds, releaseSeconds)
COMPONENT_PARAMS_DEFINE_TYPE(BoneOffsetConfig, chains)
