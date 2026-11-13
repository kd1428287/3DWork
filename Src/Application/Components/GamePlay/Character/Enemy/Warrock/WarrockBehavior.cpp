#include "WarrockBehavior.h"
#include "../EnemyAIController.h"
#include "../EnemyActions.h"
#include "Application/Definitions/Character/Enemy/EnemyBlackboardKeys.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTComposite.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTCommittedSelector.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTBlackboardConditions.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTPendingReactionAction.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTWeightedAttackAction.h"
#include "../../Common/AttackSourceComponent.h"
#include "../../../../Graphics/Render/Mod/InkStyleComponent.h"

// --- BT組み立て -----------------------------------------------------------
std::unique_ptr<IBTNode<EnemyAIController>> WarrockBehavior::BuildTree(EnemyAIController* /*owner*/)
{
	// 間合い詰め(近接プール外)と通常攻撃。開始したら終了まで他の行動に遮られない
	auto attackGroup = std::make_unique<BTCommittedSelector<EnemyAIController>>();
	attackGroup->AddChild(BuildAttackSeq(EnemyBlackboardKeys::IsTargetInGapCloserRange,
		[](EnemyAIController* c) { return c->ChooseGapCloserAttack(); }));
	attackGroup->AddChild(BuildAttackSeq(EnemyBlackboardKeys::IsTargetInAttackRange,
		[](EnemyAIController* c) { return c->ChooseAttack(); }));

	auto chaseSeq = std::make_unique<BTSequence<EnemyAIController>>();
	chaseSeq->AddChild(std::make_unique<BTCompareBoolCondition<EnemyAIController>>(
		EnemyBlackboardKeys::HasTarget, true));
	chaseSeq->AddChild(std::make_unique<EnemyActionMaintainDistance>());

	// 優先度順: 割り込み演出 > 攻撃 > 追跡 > 待機。攻撃を中断できるのは割り込み演出だけ
	auto root = std::make_unique<BTSelector<EnemyAIController>>();
	root->AddChild(BuildReactionSeq());
	root->AddChild(std::move(attackGroup));
	root->AddChild(std::move(chaseSeq));
	root->AddChild(std::make_unique<EnemyActionIdle>());
	return root;
}

std::unique_ptr<IBTNode<EnemyAIController>> WarrockBehavior::BuildReactionSeq()
{
	// 条件はBlackboardのキーだけで判定する(エディタのノードグラフでも表現できるように)
	auto seq = std::make_unique<BTSequence<EnemyAIController>>();
	seq->AddChild(std::make_unique<BTCompareBoolCondition<EnemyAIController>>(
		EnemyBlackboardKeys::HasPendingReaction, true));
	seq->AddChild(std::make_unique<BTPendingReactionAction<EnemyAIController>>(
		[this](EnemyAIController* c) { return ResolvePendingClip(c); },
		[this](EnemyAIController* c) { ClearPending(c); }));
	return seq;
}

std::unique_ptr<IBTNode<EnemyAIController>> WarrockBehavior::BuildAttackSeq(
	const std::string& rangeKey, std::function<const EnemyAttackDefinition*(EnemyAIController*)> chooser)
{
	// Sequenceは全子がSuccessで成功するため、Condition2本を並べるだけでAND条件になる
	auto seq = std::make_unique<BTSequence<EnemyAIController>>();
	seq->AddChild(std::make_unique<BTCompareBoolCondition<EnemyAIController>>(rangeKey, true));
	seq->AddChild(std::make_unique<BTCompareBoolCondition<EnemyAIController>>(
		EnemyBlackboardKeys::IsAttackOnCooldown, false));
	seq->AddChild(std::make_unique<BTWeightedAttackAction<EnemyAIController>>(std::move(chooser)));
	return seq;
}

// --- IEnemyBehavior -------------------------------------------------------
void WarrockBehavior::OnSpawned(EnemyAIController* owner)
{
	RequestReaction(owner, "Roar", ReactionPriority::Roar);
	owner->PlaySE(owner->GetSoundData().roar);
}

void WarrockBehavior::OnStaggered(EnemyAIController* owner, bool isLarge, float /*duration*/)
{
	// 体幹ダメージと崩壊判定はHitReactionComponent側で済んでいる(isLargeがその結果)
	RequestReaction(owner, isLarge ? "BigStagger" : "HitReaction",
		isLarge ? ReactionPriority::BigStagger : ReactionPriority::HitReaction);
}

void WarrockBehavior::OnAttackDeflected(EnemyAIController* owner, const Events::AttackDeflected& event)
{
	RequestReaction(owner, "AttackDeflected", ReactionPriority::Deflected);

	if (PostureComponent* posture = owner->GetPostureComponent()) {
		posture->AddPostureDamage(event.parryPostureDamage);
		if (posture->IsBroken()) {
			// 通常被弾はHitReactionComponentがResetするが、この経路には無いためここで行う
			posture->Reset();
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

void WarrockBehavior::OnFatalized(EnemyAIController* owner)
{
	return;	// 現在は無効化中。以下は有効化時の処理

	owner->StopMovement();

	const auto& table = owner->GetData().oneShotAnimations;
	const auto it = table.find("Fatalized");
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

// --- 割り込み演出 ---------------------------------------------------------
void WarrockBehavior::RequestReaction(EnemyAIController* owner, const std::string& oneShotAnimationKey, ReactionPriority priority)
{
	if (pending_.oneShotAnimationKey.empty() || priority < pending_.priority) {
		pending_ = { oneShotAnimationKey, priority };
		if (owner != nullptr) {
			owner->GetBlackboard().SetBool(EnemyBlackboardKeys::HasPendingReaction, true);
		}
	}
}

const MotionClipData* WarrockBehavior::ResolvePendingClip(EnemyAIController* owner)
{
	if (pending_.oneShotAnimationKey.empty()) return nullptr;

	const auto& table = owner->GetData().oneShotAnimations;
	const auto it = table.find(pending_.oneShotAnimationKey);
	if (it != table.end()) return &it->second;

	// 未定義キーは捨てる(残すと以降の低優先の要求が全て弾かれるため)
	ClearPending(owner);
	return nullptr;
}

void WarrockBehavior::ClearPending(EnemyAIController* owner)
{
	pending_ = PendingReaction();
	owner->GetBlackboard().SetBool(EnemyBlackboardKeys::HasPendingReaction, false);
}
