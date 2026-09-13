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

	// schema(スカラー値の自動描画)では表現できない構造(可変長のリスト等)を持つ
	// コンポーネント向けに、Inspector描画そのものを差し替えられるようにするフック。
	// 設定されていれば、MapEditor::DrawComponentList()はschemaの自動描画より
	// こちらを優先する。
	//   第1引数: このコンポーネントのparams(json)。中身を自由に書き換えてよい
	//   第2引数: 「今から編集を始めるので、直前の状態をUndoスタックへ積んでほしい」
	//            というリクエスト用コールバック(呼び出し側のPushUndo()に繋がる)。
	//            値を書き換える手前でIsItemActivated()等と組み合わせて呼ぶこと
	//   戻り値 : このフレームで何か編集されたか
	//
	// ImGuiに依存するため、これを実際にセットするのはComponentRegistrations.cppではなく
	// MapEditor.cpp側にする(Runtime専用ファイルにEditor依存を持ち込まないため)。
	// ComponentRegistry::SetCustomInspector()経由で、登録後に後付けする
	std::function<bool(nlohmann::json&, const std::function<void()>& requestUndoCheckpoint)> drawCustomInspector;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ここから、コンポーネント登録時の「schema・defaultParams・JSON変換」三重管理を無くすための
// 最小限のリフレクション機構。
//	※ Math::Vector3 を直接使っているため、このヘッダは(MapData.h等それを使う他のヘッダと
//	  同様に)Mathエイリアス名前空間が見える状態でincludeされる前提(PCH経由)。
//
// 使い方(ComponentRegistrations.cppでの例):
//
//   struct ColliderParams
//   {
//       Math::Vector3 halfExtents = { 0.5f, 0.5f, 0.5f };
//       Math::Vector3 offset      = { 0.0f, 0.0f, 0.0f };
//
//       static const std::vector<ReflectedField<ColliderParams>>& Fields()
//       {
//           static std::vector<ReflectedField<ColliderParams>> fields = {
//               MakeField("halfExtents", "Half Extents", &ColliderParams::halfExtents),
//               MakeField("offset",      "Offset",       &ColliderParams::offset),
//           };
//           return fields;
//       }
//   };
//
//   RegisterComponent<ColliderParams>("Collider", [](GameObject& obj, const ColliderParams& p)
//   {
//       obj.AddComponent<ColliderComponent>()->AddBox("body", p.halfExtents, p.offset, ...);
//   });
//
// キー名("halfExtents"等)はFields()の中に1回書くだけでよく、schema・defaultParams・
// json⇔構造体変換のすべてがそこから自動的に揃う。createラムダは型安全なメンバアクセス
// (p.halfExtents)だけで書け、生のjsonキー文字列を再び登場させる必要が無い。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

// C++の型 → ParamType の対応。新しい型を扱えるようにしたい時は、ここに特殊化を1つ追加し、
// 下のToJsonValue/FromJsonValueにもオーバーロードを追加する
template <typename T> struct ParamTypeOf;
template <> struct ParamTypeOf<float> { static constexpr ParamType value = ParamType::Float; };
template <> struct ParamTypeOf<std::string> { static constexpr ParamType value = ParamType::String; };
template <> struct ParamTypeOf<bool> { static constexpr ParamType value = ParamType::Bool; };
// Math::Vector3の特殊化はMath名前空間が見えるTU側(ComponentRegistrations.cpp等)で
// このヘッダをincludeした後に書ける想定だが、頻用のため代表してここにも用意しておく
template <> struct ParamTypeOf<Math::Vector3> { static constexpr ParamType value = ParamType::Vector3; };

inline nlohmann::json ToJsonValue(float v) { return v; }
inline nlohmann::json ToJsonValue(const std::string& v) { return v; }
inline nlohmann::json ToJsonValue(bool v) { return v; }
inline nlohmann::json ToJsonValue(const Math::Vector3& v) { return { v.x, v.y, v.z }; }

template <typename T> T FromJsonValue(const nlohmann::json& j);
template <> inline float FromJsonValue<float>(const nlohmann::json& j) { return j.get<float>(); }
template <> inline std::string FromJsonValue<std::string>(const nlohmann::json& j) { return j.get<std::string>(); }
template <> inline bool FromJsonValue<bool>(const nlohmann::json& j) { return j.get<bool>(); }
template <> inline Math::Vector3 FromJsonValue<Math::Vector3>(const nlohmann::json& j)
{
	if (!j.is_array() || j.size() < 3) return Math::Vector3(0.0f, 0.0f, 0.0f);
	return Math::Vector3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

// パラメータ構造体の1メンバぶんの情報。key/label/typeに加え、
// 「そのメンバをjsonへ/から変換する処理」まで一式持つ
template <typename Struct>
struct ReflectedField
{
	std::string	key;
	std::string	label;
	ParamType	type;

	std::function<void(const Struct&, nlohmann::json&)>	toJson;
	std::function<void(Struct&, const nlohmann::json&)>	fromJson;
};

// メンバポインタ1個から ReflectedField を組み立てる。型(FieldT)は
// メンバポインタから自動推論されるので、ParamTypeやjson変換を呼び出し側で
// 書く必要はない
template <typename Struct, typename FieldT>
ReflectedField<Struct> MakeField(std::string key, std::string label, FieldT Struct::* member)
{
	ReflectedField<Struct> field;
	field.key = std::move(key);
	field.label = std::move(label);
	field.type = ParamTypeOf<FieldT>::value;

	field.toJson = [member](const Struct& s, nlohmann::json& out)
		{
			out = ToJsonValue(s.*member);
		};
	field.fromJson = [member](Struct& s, const nlohmann::json& in)
		{
			s.*member = FromJsonValue<FieldT>(in);
		};

	return field;
}

// パラメータ構造体StructからComponentTypeInfoを組み立てて登録する。
// Struct::Fields()が返すリフレクション情報から、schema/defaultParamsが自動生成される。
// createは (GameObject&, const Struct&) を受け取る、型安全な生成処理だけを書けばよい
template <typename Struct>
void RegisterComponent(const std::string& typeName, std::function<void(GameObject&, const Struct&)> create);

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

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

	// 登録済みの種類に、Editor専用のカスタムInspector描画を後付けする。
	// (Register()と分けているのは、ImGuiに依存するUIコードをComponentRegistrations.cpp
	//  ではなくMapEditor.cpp側から差し込めるようにするため。未登録の種類名を渡した場合は
	//  何もしない=先にRegisterMapComponentTypes()側で登録されている前提)
	void SetCustomInspector(const std::string& typeName,
		std::function<bool(nlohmann::json&, const std::function<void()>&)> drawFn)
	{
		auto it = m_types.find(typeName);
		if (it == m_types.end()) return;
		it->second.drawCustomInspector = std::move(drawFn);
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

// RegisterComponent<Struct>()の実装本体。宣言は上のReflected~のブロックにある
// (ComponentRegistry::Instance()を使うためクラス定義の後ろに置く必要がある)
template <typename Struct>
void RegisterComponent(const std::string& typeName, std::function<void(GameObject&, const Struct&)> create)
{
	ComponentTypeInfo info;
	info.typeName = typeName;

	Struct defaults;	// デフォルト値はStructのメンバ初期化子からそのまま取る

	for (auto& field : Struct::Fields())
	{
		info.schema.push_back({ field.key, field.label, field.type });

		nlohmann::json v;
		field.toJson(defaults, v);
		info.defaultParams[field.key] = v;
	}

	info.create = [create](GameObject& obj, const nlohmann::json& params)
		{
			Struct s;	// フィールドが欠けていた場合はここでのデフォルト値が使われる
			for (auto& field : Struct::Fields())
			{
				if (params.contains(field.key))
				{
					field.fromJson(s, params[field.key]);
				}
			}
			create(obj, s);
		};

	ComponentRegistry::Instance().Register(std::move(info));
}