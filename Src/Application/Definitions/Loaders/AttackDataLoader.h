#pragma once
#include "../Character/Common/CombatData.h"
#include "nlohmann/json.hpp"

// ============================================================
// AttackData(CombatData.h)本体のJSON読み込みを担当する共通ユーティリティ。
//
// AttackDataはPlayer/Enemy両方が「1回の攻撃の中身」として共有する型
// (PlayerCombatTypes.h内のコメント参照)。PlayerAttackDefinition/
// (将来の)EnemyAttackDefinitionは、いずれもこのAttackDataを1つ持ち、
// その外側にPlayer固有(コンボ木/entryCommand)・Enemy固有(AIの選択条件等)
// のラッパー情報を添える構成になっている。
//
// JSON読み込みもこの境界に合わせ、「AttackData本体の読み方」だけを
// ここで共通化する。ラッパー部分(comboLinks/entryCommand、あるいは
// Enemy側の選択条件)は種別ごとに形が異なるため無理に汎用化せず、
// PlayerAttackTableLoader/(将来の)EnemyAttackTableLoaderがそれぞれ
// 自分のラッパー型を読んだ上で、attack部分だけこちらへ委譲する。
// ============================================================
namespace AttackDataLoader
{
	// out(既定値で初期化済み)を上書きする方式。JSON側に無いキーは
	// AttackData側のデフォルト値をそのまま残す。
	void ReadAttackData(const nlohmann::json& j, AttackData& out);
}
