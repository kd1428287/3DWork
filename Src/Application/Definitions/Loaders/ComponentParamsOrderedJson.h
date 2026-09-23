#pragma once
#include "nlohmann/json.hpp"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// コンポーネント/Prefabのparamsとして使う型のJSON変換を定義するマクロ。
//
//	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULTはnlohmann::json専用に固定されており、
//	生成されるto_json()がメンバを宣言順に書き込んでも、書き込み先(nlohmann::json)自体が
//	std::mapベースのため最終的にキー名のアルファベット順で格納されてしまう。
//	これがComponentRegistry::FindDefaultParams()の結果(MapEditor等Inspector側の表示順)が
//	A→Zになってしまっていた原因。
//
//	ComponentRegistryのdefaultParamsだけを挿入順を保持するnlohmann::ordered_jsonで
//	持たせているので、そちらへ書き込むto_json(ordered_json&, const T&)も追加で生成する。
//	nlohmann本体のマクロ部品(NLOHMANN_JSON_EXPAND/PASTE/TO)を流用しているだけで、
//	実行時にparamsを読む方のfrom_json(nlohmann::json)は元のマクロのままなので、
//	セーブ/ロードや実行時のBuild()の挙動には影響しない。
//
//	コンポーネントのparams(Config/Params)として使う型、および、その型からネストして
//	参照される型(例:PlayerStatusControllerConfig::evade→EvadeData、
//	EnemyAttackDefinition::attackData→AttackData)は、末端の型まで全てこのマクロで
//	定義すること。ネストした型のどれか1つでもordered_json版のto_json()が無いと、
//	そこだけコンパイルエラーになるか、あるいはそこだけ表示順が直らない
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
#define COMPONENT_PARAMS_DEFINE_TYPE(Type, ...)                                                \
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Type, __VA_ARGS__)                          \
	inline void to_json(nlohmann::ordered_json& nlohmann_json_j, const Type& nlohmann_json_t)   \
	{ NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) }
