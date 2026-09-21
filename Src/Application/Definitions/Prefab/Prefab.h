#pragma once

#include <string>
#include <vector>

#include "nlohmann/json.hpp"

// コンポーネント1個分のデータ。paramsの意味はtypeごとに変わる。
struct ComponentEntry
{
	std::string    type;
	nlohmann::json params = nlohmann::json::object();
};

inline void from_json(const nlohmann::json& j, ComponentEntry& e)
{
	e.type = j.at("type").get<std::string>();
	e.params = j.value("params", nlohmann::json::object());
}

inline void to_json(nlohmann::json& j, const ComponentEntry& e)
{
	j = { { "type", e.type }, { "params", e.params } };
}

// GameObject1個分の定義。マップ上の1エンティティもキャラクターもこの形で表す。
// childrenは同時に生成される別オブジェクトで、コンポーネント側からは親を参照できる(BuildContext::parent)。
struct PrefabDefinition
{
	std::string name = "GameObject";

	Math::Vector3    pos = { 0.0f, 0.0f, 0.0f };
	Math::Quaternion rotation = Math::Quaternion::Identity;
	Math::Vector3    scale = { 1.0f, 1.0f, 1.0f };

	std::vector<ComponentEntry>   components;
	std::vector<PrefabDefinition> children;
};
