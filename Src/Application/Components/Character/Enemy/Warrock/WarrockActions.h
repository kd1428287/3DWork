#pragma once
#include "../../BehaviorTree/IBTNode.h"
#include "WarrockAIData.h"

class WarrockAIController;

// ============================================================
// WarrockAIControllerが使うActionをまとめたファイル。EnemyActions.hとは
// 完全に独立したWarrock専用の実装(WarrockAIController.h冒頭コメント参照)。
// EnemyAIData/EnemyAttackDefinition型を使っているのはデータの器を
// 合わせているだけで、ロジックの共有を意図したものではない。
//
// 【現在実装済みのアニメーション】
// Idle / Dying / JumpAttack / Punch / Roaring / Run / Swipning / Kick /
// SmallReaction
// (Dyingは被弾/死亡イベントの配線が未着手のため、今回はまだ型を
//  作らずスコープ外としている)
// ============================================================

// 「待機」: Warrockは持ち場から動かないボスのため巡回は行わず、
// その場で待ち続けるだけの実装にする(EnemyActionIdleと違い、
// Patrol相当のクラスは意図的に用意しない)。Selectorの最後の子として
// 他が全てFailureの間、フォールバック先になり続ける。
class WarrockActionIdle : public IBTNode<WarrockAIController>
{
public:
	BTNodeStatus Tick(WarrockAIController* context, float deltaTime) override;
};

// 「追跡」: ターゲットの方向へ移動し続ける。HasTarget()がfalseになった
// 瞬間にFailureを返し、Selectorが次フレームで待機へ自然にフォールバック
// する。
class WarrockActionChase : public IBTNode<WarrockAIController>
{
public:
	BTNodeStatus Tick(WarrockAIController* context, float deltaTime) override;
};

// 「攻撃」: Punch/Kick/Swipning/JumpAttackの4種から重み付き抽選で1つ選び、
// Windup→Active→Recoveryの3フェーズで実行する(重みの目安: Punchが
// 最も軽く高頻度、JumpAttackが最も重く低頻度。CreateDebugWarrockAIData()
// 参照)。一度Windupに入ったらRecoveryが終わるまでやり切る。
class WarrockActionAttack : public IBTNode<WarrockAIController>
{
public:
	BTNodeStatus Tick(WarrockAIController* context, float deltaTime) override;
	void Reset() override;

private:
	enum class Phase { NotStarted, Windup, Active, Recovery };
	Phase phase_ = Phase::NotStarted;
	float elapsed_ = 0.0f;
	const EnemyAttackDefinition* current_ = nullptr;

	// Reset()がcontextを受け取れない制約への回避策。
	WarrockAIController* lastContext_ = nullptr;
};

// 「被弾リアクション」: SmallReactionを1回再生する割り込み行動。
// WarrockAIController::RequestHitReaction()で立てられた要求フラグを
// BuildTree()側のCondition経由で最優先チェックし、このノード自身が
// 再生完了のタイミングでConsumeHitReactionRequest()を呼んで要求を
// 消費する(再生中はフラグを立てたままにすることで、reactiveな
// Selectorが毎フレーム同じ枝を選び続け、リアクションが最後まで
// 中断されずに再生される)。
//
// 【未接続】現時点ではWarrockにHealthComponent/被弾イベントの購読が
// 無いため、RequestHitReaction()を実際に呼び出す経路は未実装
// (Enemy側のOnCollisionEnter相当の仕組みを将来追加した際にそこから
//  呼ぶ想定)。
class WarrockActionHitReaction : public IBTNode<WarrockAIController>
{
public:
	BTNodeStatus Tick(WarrockAIController* context, float deltaTime) override;
	void Reset() override { playing_ = false; elapsed_ = 0.0f; }

private:
	static constexpr float kReactionDuration = 0.4f; // 【要確認】実アニメーション尺に合わせて調整
	bool playing_ = false;
	float elapsed_ = 0.0f;
};

// 「咆哮」: Roaringを1回再生する割り込み行動。登場時(WarrockAIController::
// Start()から自動要求)、および(実装するかは未定の)第二フェーズ移行時の
// 両方から同じRequestRoar()経由で使い回せるよう、HitReactionと同じ
// 「要求フラグを消費する」形にしている。
class WarrockActionRoar : public IBTNode<WarrockAIController>
{
public:
	BTNodeStatus Tick(WarrockAIController* context, float deltaTime) override;
	void Reset() override { playing_ = false; elapsed_ = 0.0f; }

private:
	static constexpr float kRoarDuration = 1.5f; // 【要確認】実アニメーション尺に合わせて調整
	bool playing_ = false;
	float elapsed_ = 0.0f;
};