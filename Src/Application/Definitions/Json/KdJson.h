#pragma once

#include "nlohmann/json.hpp"

#include "Application/Definitions/Json/CoreJson.h"

// params用型のJSON変換。Inspectorの表示順を保つordered_json版to_jsonも生成する。
// Config/Paramsから辿れる末端の型まで、全てこのマクロで定義すること。
#define COMPONENT_PARAMS_DEFINE_TYPE(Type, ...)                                                \
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Type, __VA_ARGS__)                          \
	inline void to_json(nlohmann::ordered_json& nlohmann_json_j, const Type& nlohmann_json_t)   \
	{ NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) }
