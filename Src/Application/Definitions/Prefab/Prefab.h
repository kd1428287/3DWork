#pragma once
#include <string_view>
#include "nlohmann/json.hpp"

// コンポーネント1個分のデータ paramsの意味はtypeごとに変わる
struct ComponentEntry
{
	std::string    type;
	nlohmann::json params = nlohmann::json::object();

	// paramsを型付きで読み書きする(Tはfrom_json/to_jsonを持つConfig/Params型)
	template<typename T> T GetParams() const { return params.get<T>(); }
	template<typename T> void SetParams(const T& value) { params = nlohmann::json(value); }
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

// コンポーネント配列から指定typeを探す(無ければnullptr)。コンポーネント参照の唯一の入口
inline ComponentEntry* FindComponent(std::vector<ComponentEntry>& list, std::string_view type)
{
	for (auto& c : list) { if (c.type == type) return &c; }
	return nullptr;
}
inline const ComponentEntry* FindComponent(const std::vector<ComponentEntry>& list, std::string_view type)
{
	for (auto& c : list) { if (c.type == type) return &c; }
	return nullptr;
}

// JSON配列 <-> コンポーネント配列。typeが空・オブジェクトでないエントリは読み飛ばし、paramsがオブジェクトでなければ空にする
inline std::vector<ComponentEntry> ReadComponents(const nlohmann::json& array)
{
	std::vector<ComponentEntry> result;
	if (!array.is_array()) return result;

	for (auto& jc : array)
	{
		if (!jc.is_object()) continue;

		ComponentEntry c;
		c.type = jc.value("type", std::string());
		if (c.type.empty()) continue;

		if (jc.contains("params") && jc["params"].is_object()) { c.params = jc["params"]; }
		result.push_back(std::move(c));
	}
	return result;
}

inline nlohmann::json WriteComponents(const std::vector<ComponentEntry>& list)
{
	nlohmann::json array = nlohmann::json::array();
	for (auto& c : list) { array.push_back(c); }
	return array;
}

// GameObject1個分の定義
// childrenは同時に生成される別オブジェクトで、コンポーネント側からは親を参照できる(BuildContext::parent)
struct PrefabDefinition
{
	std::string name = "GameObject";

	Math::Vector3    pos = { 0.0f, 0.0f, 0.0f };
	Math::Quaternion rotation = Math::Quaternion::Identity;
	Math::Vector3    scale = { 1.0f, 1.0f, 1.0f };

	std::vector<ComponentEntry>   components;
	std::vector<PrefabDefinition> children;

	ComponentEntry* FindComponent(std::string_view type) { return ::FindComponent(components, type); }
	const ComponentEntry* FindComponent(std::string_view type) const { return ::FindComponent(components, type); }
};
