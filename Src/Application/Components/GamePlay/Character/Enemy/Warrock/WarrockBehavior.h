#pragma once
#include "../StandardEnemyBehavior.h"

// Warrock(武器を持たない大型モンスター)専用の判断層。
// 割り込み演出などの共通処理はStandardEnemyBehaviorが担い、ここはBT構成と登場時の咆哮だけを持つ。
class WarrockBehavior : public StandardEnemyBehavior
{
public:
	std::unique_ptr<IBTNode<EnemyAIController>> BuildTree(EnemyAIController* owner) override;
	void OnSpawned(EnemyAIController* owner) override;
};
