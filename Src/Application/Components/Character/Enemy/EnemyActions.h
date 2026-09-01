#pragma once
#include "../BehaviorTree/IBTNode.h"
#include "EnemyAIData.h"

class EnemyAIController;

// ============================================================
// EnemyAIControllerが使うAction(待機/巡回/追跡)をまとめたファイル。
// PlayerState.h/EnemyState.hが複数のStateクラスを1ファイルにまとめて
// いるのと同じ流儀(1つのControllerに紐づく、互いに関連の深い小さな
// クラス群は1ファイルにまとめる)に合わせている。
//
// 【改名について】旧BruteActionIdle/Patrol/Chase/AttackをEnemyAI*へ改名。
// Brute専用ではなく全敵種で共用するクラスであるため
// (EnemyAIController.h冒頭コメント参照)。
//
// 【攻撃(Attack)について】
// 以前はEnemyActionAttackとしてここに実装があったが、Windup/Active/
// Recoveryの3フェーズ実行はWarrockActionAttackと完全に同一のコピーに
// なっていたため、BTWeightedAttackAction<T>(BTWeightedAttackAction.h)
// へ共通化した。EnemyAIController::BuildTree()では
// BTWeightedAttackAction<EnemyAIController>を直接使う。
// ============================================================

// 「待機」: 現在地でEnemyAIData::idleDuration秒だけ足を止める。
// 巡回地点(patrolPoints)が1つも無い場合は、時間経過による終了判定を
// 行わずRunningを返し続ける(=完全な待機AIとして機能する。この場合
// EnemyActionPatrolは一度も呼ばれない。ボスのような「持ち場から動かない」
// 敵はこれを利用する)。
class EnemyActionIdle : public IBTNode<EnemyAIController>
{
public:
	BTNodeStatus Tick(EnemyAIController* context, float deltaTime) override;
	void Reset() override { elapsed_ = 0.0f; }

private:
	float elapsed_ = 0.0f;
};

// 「巡回」: EnemyAIData::patrolPointsを順番に歩いて回る。1地点に
// 到達したらSuccessを返し(次のTickでEnemyActionIdleに戻り、また
// 待機→巡回のループが回る)、AdvanceToNextPatrolPoint()で次の目的地へ
// 進める。patrolPointsが空ならそもそも実行できないため即Failureを返す
// (この場合EnemyActionIdleが単独の待機AIとして機能し続ける)。
class EnemyActionPatrol : public IBTNode<EnemyAIController>
{
public:
	BTNodeStatus Tick(EnemyAIController* context, float deltaTime) override;

private:
	static constexpr float kArriveThreshold = 0.25f; // 到達判定の許容誤差(メートル)
};

// 「追跡」: ターゲットの方向へ移動し続ける。HasTarget()がfalseになった
// (索敵範囲の外へ出た、ヒステリシス込み)瞬間にFailureを返し、Selector側が
// 次フレームで待機/巡回へ自然にフォールバックする。
//
// 攻撃間合いに入ったかどうかはこのノード自身では判定しない。親のSelector
// 自体がreactiveで毎フレーム攻撃Sequenceから評価し直すため、間合いに
// 入った瞬間はそちらが優先されてこのノードは呼ばれなくなる
// (EnemyAIController::BuildTree()参照)。
class EnemyActionChase : public IBTNode<EnemyAIController>
{
public:
	BTNodeStatus Tick(EnemyAIController* context, float deltaTime) override;
};
