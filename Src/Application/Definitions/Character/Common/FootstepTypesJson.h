#pragma once

#include "Application/Definitions/Json/KdJson.h"
#include "Application/Definitions/Character/Common/FootstepTypes.h"

// FootSide: JSONでは"Left"/"Right"の文字列で指定する。未知の値はLeftになる。
NLOHMANN_JSON_SERIALIZE_ENUM(FootSide, {
	{ FootSide::Left, "Left" },
	{ FootSide::Right, "Right" },
	})

COMPONENT_PARAMS_DEFINE_TYPE(FootstepTrigger, phase, foot)
