#include "WarrockBehavior.h"
#include "../EnemyAIController.h"
#include "../EnemyActions.h"
#include "Application/Definitions/Character/Enemy/EnemyBlackboardKeys.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTComposite.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTBlackboardConditions.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTPendingReactionAction.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTWeightedAttackAction.h"
#include "../../Common/AttackSourceComponent.h"

std::unique_ptr<IBTNode<EnemyAIController>> WarrockBehavior::BuildTree(EnemyAIController* /*owner*/)
{
	// 被弾リアクション/大スタン/パリィされた時のリアクション/咆哮は、
	// 「今どの演出が要求されているか」をpending_という単一の状態として
	// 持ち、BTPendingReactionAction<T>1本でまとめて処理する
	// (WarrockBehavior.hクラス冒頭コメント参照)。優先度の解決は
	// RequestReaction()側で済んでいるため、ここではBlackboard
	// (EnemyBlackboardKeys::HasPendingReaction)の有無だけを条件に
	// 見ればよい。
	//
	// 【変更】以前は「[this](EnemyAIController*){ return
	// !pending_.oneShotAnimationKey.empty(); }」というWarrockBehavior
	// 内部の状態を直接見るラムダ条件だったが、ノードグラフエディタが
	// 組む条件はC++の型やメンバを知らない「キー名の一致」だけで
	// 表現できる必要があるため、BTCompareBoolCondition<T>へ置き換えた。
	// pending_自体はBTPendingReactionActionのresolveClipラムダ
	// (以前と同じく[this]でWarrockBehaviorを直接参照)が引き続き読む。
	auto reactionSeq = std::make_unique<BTSequence<EnemyAIController>>();
	reactionSeq->AddChild(std::make_unique<BTCompareBoolCondition<EnemyAIController>>(
		EnemyBlackboardKeys::HasPendingReaction, true));
	reactionSeq->AddChild(std::make_unique<BTPendingReactionAction<EnemyAIController>>(
		[this](EnemyAIController* c) -> const MotionClipData* {
			if (pending_.oneShotAnimationKey.empty()) return nullptr;
			const auto& table = c->GetData().oneShotAnimations;
			const auto it = table.find(pending_.oneShotAnimationKey);
			return (it != table.end()) ? &it->second : nullptr;
		},
		[this](EnemyAIController* c) {
			pending_ = PendingReaction();
			c->GetBlackboard().SetBool(EnemyBlackboardKeys::HasPendingReaction, false);
		}));

	// 【変更】以前は「c->IsTargetInAttackRange() && !c->IsAttackOnCooldown()」
	// という2つの条件をまとめた1本のラムダだったが、BTSequenceは
	// 「子が全員Successして初めてこのSequence自身もSuccess」という
	// AND相当の意味を元々持っているため、複合条件を1本のノードに
	// まとめる必要は無い。単純にCondition2本を並べるだけでAND条件を
	// 表現できる(専用のANDノードを新設する必要も無かった)。
	// 間合い詰め専用。近接プール外(遠距離)でのみ発火する。
	auto gapCloserSeq = std::make_unique<BTSequence<EnemyAIController>>();
	gapCloserSeq->AddChild(std::make_unique<BTCompareBoolCondition<EnemyAIController>>(
		EnemyBlackboardKeys::IsTargetInGapCloserRange, true));
	gapCloserSeq->AddChild(std::make_unique<BTCompareBoolCondition<EnemyAIController>>(
		EnemyBlackboardKeys::IsAttackOnCooldown, false));
	gapCloserSeq->AddChild(std::make_unique<BTWeightedAttackAction<EnemyAIController>>(
		[](EnemyAIController* c) { return c->ChooseGapCloserAttack(); }));

	auto attackSeq = std::make_unique<BTSequence<EnemyAIController>>();
	attackSeq->AddChild(std::make_unique<BTCompareBoolCondition<EnemyAIController>>(
		EnemyBlackboardKeys::IsTargetInAttackRange, true));
	attackSeq->AddChild(std::make_unique<BTCompareBoolCondition<EnemyAIController>>(
		EnemyBlackboardKeys::IsAttackOnCooldown, false));
	attackSeq->AddChild(std::make_unique<BTWeightedAttackAction<EnemyAIController>>(
		[](EnemyAIController* c) { return c->ChooseAttack(); }));

	auto chaseSeq = std::make_unique<BTSequence<EnemyAIController>>();
	chaseSeq->AddChild(std::make_unique<BTCompareBoolCondition<EnemyAIController>>(
		EnemyBlackboardKeys::HasTarget, true));
	chaseSeq->AddChild(std::make_unique<EnemyActionMaintainDistance>());

	// 優先度順。
	auto selector = std::make_unique<BTSelector<EnemyAIController>>();
	selector->AddChild(std::move(reactionSeq));
	selector->AddChild(std::move(gapCloserSeq));
	selector->AddChild(std::move(attackSeq));
	selector->AddChild(std::move(chaseSeq));
	selector->AddChild(std::make_unique<EnemyActionIdle>());

	return selector;
}

void WarrockBehavior::OnSpawned(EnemyAIController* owner)
{
	RequestReaction(owner, "Roar", ReactionPriority::Roar);
}

void WarrockBehavior::OnStaggered(EnemyAIController* owner, bool isLarge, float /*duration*/)
{
	// 体幹ダメージの適用・体幹崩壊判定はHitReactionComponent(Player/Enemy
	// 共通)側で既に完了しており、isLargeにその結果が入っている
	// (IEnemyBehavior.h::OnStaggered()コメント参照)。以前ここにあった
	// PostureComponent::AddPostureDamage()/IsBroken()の呼び出しは、
	// HitReactionComponentへの統合に伴い重複になるため削除した。
	RequestReaction(owner, isLarge ? "BigStagger" : "HitReaction",
		isLarge ? ReactionPriority::BigStagger : ReactionPriority::HitReaction);
}

// --- 被パリィ処理 ---------------------------------------------------------
void WarrockBehavior::OnParried(EnemyAIController* owner, const AttackSourceComponent::ParriedEvent& event)
{
	RequestReaction(owner, "Parried", ReactionPriority::Parried);

	if (PostureComponent* posture = owner->GetPostureComponent()) {
		posture->AddPostureDamage(event.parryPostureDamage);
		if (posture->IsBroken()) {
			RequestReaction(owner, "BigStagger", ReactionPriority::BigStagger);
		}
	}
}

void WarrockBehavior::OnDied(EnemyAIController* owner)
{
	owner->StopMovement();

	const auto& table = owner->GetData().oneShotAnimations;
	const auto it = table.find("Dying");
	if (it != table.end()) {
		owner->PlayAnimation(it->second.animationName, false, it->second.duration, it->second.useRootMotion);
	}
}

float WarrockBehavior::GetDespawnDelay(const EnemyAIController* owner) const
{
	const auto& data = owner->GetData();
	const auto it = data.oneShotAnimations.find("Dying");
	const float dyingDuration = (it != data.oneShotAnimations.end()) ? it->second.duration : 0.0f;
	return dyingDuration + data.postDeathLingerSeconds;
}

void WarrockBehavior::RequestReaction(EnemyAIController* owner, const std::string& oneShotAnimationKey, ReactionPriority priority)
{
	if (pending_.oneShotAnimationKey.empty() || priority < pending_.priority) {
		pending_ = { oneShotAnimationKey, priority };
		if (owner != nullptr) {
			owner->GetBlackboard().SetBool(EnemyBlackboardKeys::HasPendingReaction, true);
		}
	}
}
