#pragma once


#include "Application/Definitions/Prefab/Prefab.h"

class GameObject;
class ObjectManager;

// コンポーネント生成時に登録側へ渡す文脈。parentはchildrenとして生成された場合の親(ルートならnullptr)。
struct BuildContext
{
	ObjectManager& objectManager;
	GameObject& self;
	GameObject* parent = nullptr;

	// コンポーネントを追加する。同じ型が既にある場合は例外にする(登録では必ずこれを使う)。
	template<typename T, typename G = GameObject, typename... Args>
	T* Add(Args&&... args)
	{
		G& obj = self;
		if (obj.template HasComponent<T>()) throw std::runtime_error(std::string("duplicate component: ") + typeid(T).name());
		return obj.template AddComponent<T>(std::forward<Args>(args)...);
	}
};

// TがConfig型(T::Config)を持つかどうか。
template<typename T, typename = void> struct HasConfig : std::false_type {};
template<typename T> struct HasConfig<T, std::void_t<typename T::Config>> : std::true_type {};

// Prefabの"type"名からコンポーネントの生成処理を引く表。登録はComponentRegistrations.cppに書く。
class ComponentRegistry
{
public:
	using Factory = std::function<void(BuildContext&, const nlohmann::json&)>;
	using RawFunction = void (*)(BuildContext&, const nlohmann::json&);

	// 初回の呼び出し時にRegisterAllComponents()で全登録を行う。
	static ComponentRegistry& Instance();

	// 標準の登録。T::Configと、SetConfig(const Config&)を持つ型はparamsをConfigとして読み、持たない型はそのまま追加する。
	template<typename T>
	void Add(const std::string& type)
	{
		if constexpr (HasConfig<T>::value) {
			using Config = typename T::Config;
			Register(type, [](auto& ctx, const nlohmann::json& params) {
				const Config config = params.get<Config>();
				ctx.template Add<T>()->SetConfig(config);
				}, nlohmann::ordered_json(Config{}));
		}
		else {
			Register(type, [](auto& ctx, const nlohmann::json&) { ctx.template Add<T>(); });
		}
	}

	// paramsを型付きで受け取り、他のコンポーネントとの接続などの追加処理も書ける。paramsの初期値はParams{}から作る。
	template<typename Params>
	void AddWithParams(const std::string& type, std::function<void(BuildContext&, const Params&)> build)
	{
		Register(type, [build](BuildContext& ctx, const nlohmann::json& params) {
			build(ctx, params.get<Params>());
			}, nlohmann::ordered_json(Params{}));
	}

	// paramsを直接扱う登録。関数ポインタに限定して、型付きラムダの取り違えをコンパイルエラーにする。
	void AddRaw(const std::string& type, RawFunction function, nlohmann::ordered_json defaultParams = nlohmann::ordered_json::object())
	{
		Register(type, function, std::move(defaultParams));
	}

	// entryを実体化する。未登録のtypeやparamsの不正は例外にする。
	void Build(BuildContext& ctx, const ComponentEntry& entry) const;

	// --- エディタ向け ---
	// 登録済みの型名(名前順)。
	std::vector<std::string> GetTypeNames() const;

	// 型のparamsの初期値。未登録ならnullptr。
	//	表示順(挿入順)を保持したいのでordered_jsonで持つ(entry.params自体は
	//	従来通りnlohmann::jsonのままでよい。詳細はComponentRegistrations.cppの
	//	COMPONENT_PARAMS_DEFINE_TYPEマクロのコメントを参照)
	const nlohmann::ordered_json* FindDefaultParams(const std::string& type) const;

private:
	void Register(const std::string& type, Factory factory, nlohmann::ordered_json defaultParams = nlohmann::ordered_json::object())
	{
		types_[type] = { std::move(factory), std::move(defaultParams) };
	}

	struct TypeInfo
	{
		Factory                factory;
		nlohmann::ordered_json defaultParams;
	};

	std::unordered_map<std::string, TypeInfo> types_;
};

// 全コンポーネントの登録(ComponentRegistrations.cpp)。
void RegisterAllComponents(ComponentRegistry& registry);