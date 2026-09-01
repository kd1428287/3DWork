#pragma once
#include "../IEnemyBehavior.h"
#include "../EnemyActions.h"

// ============================================================
// 汎用敵(EnemyType::Brute)の判断層。旧EnemyAIController::BuildTree()
// の内容をそのままIEnemyBehaviorへ移した実装。
//
// 【現状について】WarrockBehaviorとは異なり、この実装はまだ現行の
// EnemyActions.h(Idle/Patrol/Chase)のまま。被弾リアクションのような
// 割り込み行動は、Warrock側で先行して詳細を詰めている最中のため、
// 今回はまだこちらへ展開していない(IEnemyBehavior.h冒頭コメント参照)。
// ============================================================
class BruteBehavior : public IEnemyBehavior
{
public:
	std::unique_ptr<IBTNode<EnemyAIController>> BuildTree(EnemyAIController* owner) override;

	// 体幹削り(パリィ/ガードの削り合い)はBruteには存在するがWarrockには
	// 無いため、共通処理には含めずここで行う(EnemyAIController::
	// GetPostureComponent()参照)。
	void OnHit(EnemyAIController* owner, const AttackSourceComponent& attack) override;
};
