#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "nlohmann/json.hpp"

class GameObject;

// Prefab(JSONのcomponents配列)の"type"名から、コンポーネントの生成関数を引く表。
// 新しいコンポーネントはComponentRegistrations.cppに1行足せば、Prefabから使える。
class ComponentRegistry
{
public:
	using Factory = std::function<void(GameObject*, const nlohmann::json&)>;

	static ComponentRegistry& Get();

	void Register(const std::string& type, Factory factory) { factories_[type] = std::move(factory); }

	// SetConfig(const Config&)を持つコンポーネント。エントリ自身のキーをConfigとして読む。
	template<typename T, typename Config>
	void Register(const std::string& type)
	{
		Register(type, [](auto* obj, const nlohmann::json& j) {
			const Config config = j.get<Config>();
			obj->template AddComponent<T>()->SetConfig(config);
		});
	}

	// 設定を持たないコンポーネント。
	template<typename T>
	void RegisterSimple(const std::string& type)
	{
		Register(type, [](auto* obj, const nlohmann::json&) { obj->template AddComponent<T>(); });
	}

	// componentsの各要素({"type": ..., 設定のキー...})を順にobjへ追加する。
	// 不正なエントリはログに出して飛ばし、1つでも失敗したらfalseを返す。
	bool AddComponents(GameObject* obj, const nlohmann::json& components) const;

private:
	std::unordered_map<std::string, Factory> factories_;
};

// 全コンポーネントの登録(ComponentRegistrations.cpp)。
void RegisterAllComponents(ComponentRegistry& registry);
