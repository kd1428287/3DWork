#pragma once

#include "Application/Definitions/Character/Enemy/EnemyAIData.h" // EnemyAttackDefinition
#include <string>
#include <vector>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Enemy1体分の攻撃データ(attacks/gapCloserAttacks)を、Prefab本体(Warrock.json等)とは
// 別ファイルへ外出しするためのテーブル。PlayerAttackTable(PlayerAttackTableLoader.h)と
// 同じ動機で、EnemyAIData::attacks/gapCloserAttacksと全く同じ形の配列をそのまま2本持つ。
// コンボはid/comboLinksで技を参照する(idはattacks/gapCloserAttacks横断で一意)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct EnemyAttackTable
{
	std::vector<EnemyAttackDefinition> attacks;
	std::vector<EnemyAttackDefinition> gapCloserAttacks;

	// 両プールを横断してidで技を探す。無ければnullptr。
	const EnemyAttackDefinition* FindAttack(const std::string& id) const
	{
		return FindAttackInPools(attacks, gapCloserAttacks, id);
	}

	// idの重複と、comboLinksの参照先不在を検出する。
	bool Validate() const { return ValidateAttackPools(attacks, gapCloserAttacks); }
};

class EnemyAttackTableLoader
{
public:
	// 失敗時はfalseを返し、outは変更しない
	static bool LoadFromFile(const std::string& path, EnemyAttackTable& out);
};
