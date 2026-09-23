#pragma once

#include "Application/Definitions/Character/Enemy/EnemyAIData.h" // EnemyAttackDefinition
#include <string>
#include <vector>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Enemy1体分の攻撃データ(attacks/gapCloserAttacks)を、Prefab本体(Warrock.json等)とは
// 別ファイルへ外出しするためのテーブル。PlayerAttackTable(PlayerAttackTableLoader.h)と
// 同じ動機だが、Player側と違いコンボ(id/comboLinks)は無く、EnemyAIData::attacks/
// gapCloserAttacksと全く同じ形の配列をそのまま2本持つだけの単純な構造
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct EnemyAttackTable
{
	std::vector<EnemyAttackDefinition> attacks;
	std::vector<EnemyAttackDefinition> gapCloserAttacks;
};

class EnemyAttackTableLoader
{
public:
	// 失敗時はfalseを返し、outは変更しない
	static bool LoadFromFile(const std::string& path, EnemyAttackTable& out);
};
