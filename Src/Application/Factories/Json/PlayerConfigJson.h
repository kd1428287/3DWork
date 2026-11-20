#pragma once

#include "Application/Components/GamePlay/Character/Player/PlayerCombatMovementComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerStatusController.h"
#include "Application/Components/GamePlay/Character/Player/PlayerInkGaugeComponent.h"

#include "Application/Definitions/Json/KdJson.h"
#include "Application/Definitions/Character/Player/PlayerDefinitionJson.h"

// Player専用ComponentのConfigのJSON変換。ネストする型(EvadeData等)はPlayerDefinitionJson.h側。
COMPONENT_PARAMS_DEFINE_TYPE(PlayerCombatMovementConfig, walkSpeed, runSpeed)
COMPONENT_PARAMS_DEFINE_TYPE(PlayerStatusControllerConfig, evade, guard, charge, stagger)
COMPONENT_PARAMS_DEFINE_TYPE(InkGaugeConfig, max, regenPerSecond, gainOnParry, chargeLine)
