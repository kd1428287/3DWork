#pragma once
#include "../BehaviorTree/IBTNode.h"
#include "EnemyAIData.h"

class EnemyAIController;

// ============================================================
// EnemyAIControllerが使う4つのAction(待機/巡回/追跡/攻撃)をまとめた
// ファイル。PlayerState.h/EnemyState.hが複数のStateクラスを1ファイルに
// まとめているのと同じ流儀(1つのControllerに紐づく、互いに関連の深い
// 小さなクラス群は1ファイルにまとめる)に合わせている。
//
// 【改名について】旧BruteActionIdle/Patrol/Chase/AttackをEnemyAI*へ改名。
// Brute専用ではなく全敵種で共用するクラスであるため
// (EnemyAIController.h冒頭コメント参照)。
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

// 「攻撃」: 現在の距離で使える攻撃パターン(EnemyAIData::attacks)から
// 重み付き抽選で1つ選び、Windup→Active→Recoveryの3フェーズで実行する。
// 攻撃パターンを増やす場合はEnemyAIData::attacksに要素を追加するだけで
// よく、このノード自体の変更は不要(拡張性の担保。Bossの多彩な攻撃も
// これで表現する)。
//
// 一度攻撃を開始したら(Windupに入ったら)、Recoveryが終わるまで
// 選んだ技を最後までやり切る(Player/EnemyのStateAttackと同じ
// 「一度コミットしたら踏みとどまる」設計)。
//
// 【IBTNode::Reset()の制約について】
// IBTNode<T>::Reset()はcontext引数を受け取らない仕様のため、Windup/Active
// 中に(将来実装される被弾リアクション等で)中断された場合、本来なら
// SetWeaponHitBoxEnabled(false)でHitBoxを閉じたいが、Reset()の中からは
// controllerを直接参照できない。そのためTick()の冒頭で直前のcontextを
// lastContext_へキャッシュしておき、Reset()からはそれを使う回避策を
// 取っている。
class EnemyActionAttack : public IBTNode<EnemyAIController>
{
public:
	BTNodeStatus Tick(EnemyAIController* context, float deltaTime) override;
	void Reset() override;

private:
	enum class Phase { NotStarted, Windup, Active, Recovery };
	Phase phase_ = Phase::NotStarted;
	float elapsed_ = 0.0f;
	const EnemyAttackDefinition* current_ = nullptr;

	// Reset()がcontextを受け取れない制約への回避策(クラス冒頭コメント参照)。
	EnemyAIController* lastContext_ = nullptr;
};
