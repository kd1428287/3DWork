#pragma once
#include "../BehaviorTree/IBTNode.h"

class EnemyBTController; // 前方宣言(Tick引数の型としてのみ必要なため)

// ============================================================
// 攻撃を開始し、完了するまで見届けるAction。
// 実際の攻撃モーション進行(Windup/Active/Recovery)は全て
// EnemyStatusController::TryStartAttack()以降、内部のStateパターン
// (EnemyStateAttack)が自律的に処理する。このノードはそれを
// 呼び出して結果を見ているだけの薄いラッパー。
//
// EnemyBTController側の実装(GetStatusController呼び出し)に依存するため、
// 循環include回避のためTick()の実体はEnemyActionAttack.cpp側に置く
// (EnemyActionIdle.h/.cppと同じ考え方)。
// ============================================================
class EnemyActionAttack : public IBTNode<EnemyBTController>
{
public:
	BTNodeStatus Tick(EnemyBTController* context, float deltaTime) override;
};
