#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "nlohmann/json.hpp"

class GameObject;

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップ上の1オブジェクトに付けられる「コンポーネント1個分」のデータ。
// 中身(params)の意味は type によって変わる自由形式のJSONとして持つ。
// MapEditor(編集)・MapLoader/TerrainFactory(実体化)の両方で共通して使うスキーマ
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct ComponentEntry
{
	std::string		type;				// ComponentRegistryに登録されているキー名("Collider"等)
	nlohmann::json	params = nlohmann::json::object();	// 種類ごとに意味が変わるパラメータ
};

// Inspectorでの自動UI描画に使うパラメータの型タグ(v1はこの4種類のみ)
enum class ParamType
{
	Float,
	Vector3,
	String,
	Bool,
};

// 1パラメータぶんのスキーマ定義(Inspectorの自動描画に使う。値そのものはparams側が持つ)
struct ParamSchema
{
	std::string	key;	// params内のJSONキー
	std::string	label;	// Inspector表示名
	ParamType	type;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// コンポーネント1種類ぶんの登録情報。
//	・schema/defaultParams : Editor側(Inspectorの自動描画・Add Component時の初期値)
//	・create                : Runtime側(実際のGameObjectへAddComponentする処理)
// 1つの登録情報の中にEditor/Runtime両方の関心を同居させているのは、
// MapEditorと実行時ゲームが同一ビルドターゲットで、実コンポーネントのヘッダーを
// 双方から直接includeできる構成である前提のため。別ビルドターゲットに分割する場合は
// createの部分だけRuntime側の別ファイルに分離すること
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct ComponentTypeInfo
{
	std::string					typeName;
	nlohmann::json				defaultParams = nlohmann::json::object();
	std::vector<ParamSchema>	schema;

	// 実際のGameObjectにコンポーネントを追加する処理(Runtime/シーン読み込み時に呼ばれる)
	std::function<void(GameObject&, const nlohmann::json&)> create;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// コンポーネント種類の登録簿(シングルトン)。
//	MapEditor(Add Componentの選択肢・Inspector描画)とTerrainFactory(実体化)の
//	両方がこれを介して同じ「種類名→振る舞い」の対応を参照する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class ComponentRegistry
{
public:
	static ComponentRegistry& Instance()
	{
		static ComponentRegistry instance;
		return instance;
	}

	void Register(ComponentTypeInfo info)
	{
		m_types[info.typeName] = std::move(info);
	}

	const ComponentTypeInfo* Find(const std::string& type) const
	{
		auto it = m_types.find(type);
		return it != m_types.end() ? &it->second : nullptr;
	}

	const std::unordered_map<std::string, ComponentTypeInfo>& All() const { return m_types; }

private:
	ComponentRegistry() = default;
	std::unordered_map<std::string, ComponentTypeInfo> m_types;
};

// 実際のコンポーネント種類をComponentRegistryへ登録する(ComponentRegistrations.cppで定義)。
// MapEditor起動時・ゲーム起動時のどちらから先に呼ばれても安全なように、
// 内部で2重登録防止のガードを持つ
void RegisterMapComponentTypes();
