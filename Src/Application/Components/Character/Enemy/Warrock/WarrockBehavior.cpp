#include "WarrockBehavior.h"
#include "../../BehaviorTree/BTWeightedAttackAction.h"
#include "../EnemyAIController.h"
#include "../../BehaviorTree/BTComposite.h"
#include "../../BehaviorTree/BTCondition.h"
#include "../../BehaviorTree/BTOneShotAnimationAction.h"
#include "../../../Collision/AttackSourceComponent.h"

namespace
{
	// 各種割り込み/死亡演出の再生尺。実アニメーションの尺に合わせて
	// 調整すること(【要確認】、値自体はまだ未検証)。
	constexpr float kHitReactionDuration = 0.4f;
	constexpr float kRoarDuration = 1.5f;
	constexpr float kDyingDuration = 2.0f;

	// Dyingアニメーション終了後、さらに少し間を置いてから消滅させる
	// (ボスの死亡演出として唐突に消えないようにするための余白)。
	constexpr float kPostDeathLingerSeconds = 1.0f;
}

std::unique_ptr<IBTNode<EnemyAIController>> WarrockBehavior::BuildTree(EnemyAIController* /*owner*/)
{
	// 被弾リアクション/咆哮は「要求フラグが立ったら1本再生して消費する」
	// という同じ形の割り込み行動のため、汎用BTOneShotAnimationAction<T>
	// (BTOneShotAnimationAction.h参照)をラムダで用途ごとに使い分ける形
	// にしている。フラグ自体はこのWarrockBehaviorインスタンスのメンバ
	// (クラス冒頭コメント参照)。
	auto reactionSeq = std::make_unique<BTSequence<EnemyAIController>>();
	reactionSeq->AddChild(std::make_unique<BTCondition<EnemyAIController>>(
		[this](EnemyAIController*) { return hitReactionPending_; }));
	reactionSeq->AddChild(std::make_unique<BTOneShotAnimationAction<EnemyAIController>>(
		[](EnemyAIController* c) {
			c->StopMovement();
			c->PlayAnimation("SmallReaction", false, kHitReactionDuration);
		},
		kHitReactionDuration,
		[this](EnemyAIController*) { hitReactionPending_ = false; }));

	auto roarSeq = std::make_unique<BTSequence<EnemyAIController>>();
	roarSeq->AddChild(std::make_unique<BTCondition<EnemyAIController>>(
		[this](EnemyAIController*) { return roarPending_; }));
	roarSeq->AddChild(std::make_unique<BTOneShotAnimationAction<EnemyAIController>>(
		[](EnemyAIController* c) {
			c->StopMovement();
			c->PlayAnimation("Roaring", false, kRoarDuration);
		},
		kRoarDuration,
		[this](EnemyAIController*) { roarPending_ = false; }));

	// 攻撃の3フェーズ実行(Windup/Active/Recovery)はBruteBehaviorと
	// 完全に共通のBTWeightedAttackAction<T>を使う(BTWeightedAttackAction.h
	// 参照)。「どの技を使うか」はWarrockAIData::attacksの重みで、
	// 「間合いに入ったら攻撃を試みる」という判断自体はここでWarrock
	// 固有に定義している。
	auto attackSeq = std::make_unique<BTSequence<EnemyAIController>>();
	attackSeq->AddChild(std::make_unique<BTCondition<EnemyAIController>>(
		[](EnemyAIController* c) { return c->IsTargetInAttackRange(); }));
	attackSeq->AddChild(std::make_unique<BTWeightedAttackAction<EnemyAIController>>());

	auto chaseSeq = std::make_unique<BTSequence<EnemyAIController>>();
	chaseSeq->AddChild(std::make_unique<BTCondition<EnemyAIController>>(
		[](EnemyAIController* c) { return c->HasTarget(); }));
	chaseSeq->AddChild(std::make_unique<WarrockActionChase>());

	// 優先度順(被弾リアクション > 咆哮 > 攻撃 > 追跡 > 待機)。
	// reactive版BTSelectorのため、毎フレーム必ず先頭(被弾リアクション)
	// から評価し直す。
	auto selector = std::make_unique<BTSelector<EnemyAIController>>();
	selector->AddChild(std::move(reactionSeq));
	selector->AddChild(std::move(roarSeq));
	selector->AddChild(std::move(attackSeq));
	selector->AddChild(std::move(chaseSeq));
	selector->AddChild(std::make_unique<WarrockActionIdle>());

	return selector;
}

void WarrockBehavior::OnSpawned(EnemyAIController* /*owner*/)
{
	// 登場時の咆哮を1回要求する(第二フェーズ移行時の再利用も想定。
	// クラス冒頭コメント参照)。
	roarPending_ = true;
}

void WarrockBehavior::OnHit(EnemyAIController* /*owner*/, const AttackSourceComponent& /*attack*/)
{
	hitReactionPending_ = true;
}

void WarrockBehavior::OnDied(EnemyAIController* owner)
{
	// Bossの死亡演出。Dyingアニメーションを再生し、GetDespawnDelay()で
	// 消滅までの猶予をこの尺に合わせて延長している(EnemyAIController::
	// OnDied()参照)。
	owner->StopMovement();
	owner->PlayAnimation("Dying", false, kDyingDuration);
}

float WarrockBehavior::GetDespawnDelay() const
{
	return kDyingDuration + kPostDeathLingerSeconds;
}
