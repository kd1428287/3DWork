#pragma once

#include "Application/Components/Physics/Movement/VelocityComponent.h"
#include "Application/Components/Physics/Movement/MovementComponent.h"
#include "Application/Components/Physics/Sensors/GroundSensorComponent.h"

#include "Application/Definitions/Json/KdJson.h"

// Physics系ComponentのConfigのJSON変換。
COMPONENT_PARAMS_DEFINE_TYPE(VelocityConfig, dampingPerSecond)
COMPONENT_PARAMS_DEFINE_TYPE(MovementConfig, speed)
COMPONENT_PARAMS_DEFINE_TYPE(GroundSensorConfig, footOffset, checkDistance)
