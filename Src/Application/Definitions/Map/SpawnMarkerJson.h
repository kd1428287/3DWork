#pragma once

#include "Application/Definitions/Json/KdJson.h"
#include "Application/Definitions/Map/SpawnMarker.h"

// SpawnRoleはJSONでは文字列で指定する。未知の値はNoneになる。
NLOHMANN_JSON_SERIALIZE_ENUM(SpawnRole, {
	{ SpawnRole::None, "None" },
	{ SpawnRole::Player, "Player" },
	{ SpawnRole::Enemy, "Enemy" },
	})

COMPONENT_PARAMS_DEFINE_TYPE(SpawnMarkerParams, role, prefab)
