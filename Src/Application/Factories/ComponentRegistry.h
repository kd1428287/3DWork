#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "Application/Definitions/Prefab/Prefab.h"

class GameObject;
class ObjectManager;

// コンポーネント生成時に登録側へ渡す文脈。parentはchildrenとして生成された場合の親(ルートならnullptr)。
struct BuildContext
{
	ObjectManager& objectManager;
	GameObject& self;
	GameObject* parent = nullptr;
};

// Prefabの"type"名からコンポーネントの生成処理を引く表。登録はComponentRegistrations.cppに書く。
class ComponentRegistry
{
public:
	using Factory = std::function<void(BuildContext&, const nlohmann::json&)>;

	// 初回の呼び出し時にRegisterAllComponents()で全登録を行う。
	static ComponentRegistry& Instance();

	// 自由な生成処理。defaultParamsは、エディタでコンポーネントを新規追加する時のparamsの初期値。
	void Register(const std::string& type, Factory factory, nlohmann::json defaultParams = nlohmann::json::object())
	{
		types_[type] = { std::move(factory), std::move(defaultParams) };
	}

	// paramsを持たないコンポーネント。同じ型の重複追加は例外にする。
	template<typename T>
	void Register(const std::string& type)
	{
		Register(type, [type](auto& ctx, const nlohmann::json&) {
			if (ctx.self.template HasComponent<T>()) throw std::runtime_error("duplicate component: " + type);
			ctx.self.template AddComponent<T>();
			});
	}

	// paramsをConfigとして読み、SetConfig(config)を呼ぶコンポーネント。同じ型の重複追加は例外にする。
	template<typename T, typename Config>
	void Register(const std::string& type)
	{
		Register(type, [type](auto& ctx, const nlohmann::json& params) {
			const Config config = params.get<Config>();
			if (ctx.self.template HasComponent<T>()) throw std::runtime_error("duplicate component: " + type);
			ctx.self.template AddComponent<T>()->SetConfig(config);
			}, nlohmann::json(Config{}));
	}

	// paramsを型付きで受け取る生成処理。paramsの初期値はParams{}の内容から作る。
	template<typename Params>
	void RegisterWithParams(const std::string& type, std::function<void(BuildContext&, const Params&)> build)
	{
		Register(type, [build](BuildContext& ctx, const nlohmann::json& params) {
			build(ctx, params.get<Params>());
			}, nlohmann::json(Params{}));
	}

	// entryを実体化する。未登録のtypeやparamsの不正は例外にする。
	void Build(BuildContext& ctx, const ComponentEntry& entry) const;

	// --- エディタ向け ---
	// 登録済みの型名(名前順)。
	std::vector<std::string> GetTypeNames() const;

	// 型のparamsの初期値。未登録ならnullptr。
	const nlohmann::json* FindDefaultParams(const std::string& type) const;

private:
	struct TypeInfo
	{
		Factory        factory;
		nlohmann::json defaultParams;
	};

	std::unordered_map<std::string, TypeInfo> types_;
};

// 全コンポーネントの登録(ComponentRegistrations.cpp)。
void RegisterAllComponents(ComponentRegistry& registry);