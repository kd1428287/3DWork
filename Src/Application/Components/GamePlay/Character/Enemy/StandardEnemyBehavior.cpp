#include "StandardEnemyBehavior.h"
#include "EnemyAIController.h"
#include "Application/Definitions/Character/Enemy/EnemyBlackboardKeys.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTComposite.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTBlackboardConditions.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTPendingReactionAction.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTWeightedAttackAction.h"

// --- BT組み立ての部品 -----------------------------------------------------
std::unique_ptr<IBTNode<EnemyAIController>> StandardEnemyBehavior::BuildReactionSeq()
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

std::unique_ptr<IBTNode<EnemyAIController>> StandardEnemyBehavior::BuildAttackSeq(
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
void StandardEnemyBehavior::OnStaggered(EnemyAIController* owner, bool isLarge, float /*duration*/)
{
	// 体幹ダメージと崩壊判定はHitReactionComponent側で済んでいる(isLargeがその結果)
	RequestReaction(owner, isLarge ? "BigStagger" : "HitReaction",
		isLarge ? ReactionPriority::BigStagger : ReactionPriority::HitReaction);
}

void StandardEnemyBehavior::OnAttackDeflected(EnemyAIController* owner, const Events::AttackDeflected& /*event*/)
{
	// 体幹の加算はNotifyAttacker、崩しの判定はHitReactionComponentが行うため、ここでは演出の要求だけ
	RequestReaction(owner, "AttackDeflected", ReactionPriority::Deflected);
}

void StandardEnemyBehavior::OnDied(EnemyAIController* owner)
{
	owner->StopMovement();
	PlayOneShotAnimation(owner, "Dying", false);
}

void StandardEnemyBehavior::OnBroken(EnemyAIController* owner, bool /*byHealth*/)
{
	ClearPending(owner);	// 崩れ中は通常の割り込み演出を残さない
	owner->StopMovement();
	PlayOneShotAnimation(owner, "Broken", true);	// 致命されるまでループ
}

void StandardEnemyBehavior::OnFatalized(EnemyAIController* owner)
{
	owner->StopMovement();
	PlayOneShotAnimation(owner, "Fatalized", false);
}

float StandardEnemyBehavior::GetDespawnDelay(const EnemyAIController* owner) const
{
	const auto& data = owner->GetData();
	const auto it = data.oneShotAnimations.find("Dying");
	const float dyingDuration = (it != data.oneShotAnimations.end()) ? it->second.duration : 0.0f;
	return dyingDuration + data.postDeathLingerSeconds;
}

// --- 割り込み演出 ---------------------------------------------------------
void StandardEnemyBehavior::RequestReaction(EnemyAIController* owner, const std::string& oneShotAnimationKey, ReactionPriority priority)
{
	// 演出が定義されていない敵種は要求自体を捨てる(未再生の要求が、低優先の要求や攻撃を邪魔しないように)
	if (owner != nullptr && owner->GetData().oneShotAnimations.count(oneShotAnimationKey) == 0) return;

	if (pending_.oneShotAnimationKey.empty() || priority < pending_.priority) {
		pending_ = { oneShotAnimationKey, priority };
		if (owner != nullptr) {
			owner->GetBlackboard().SetBool(EnemyBlackboardKeys::HasPendingReaction, true);
		}
	}
}

const MotionClipData* StandardEnemyBehavior::ResolvePendingClip(EnemyAIController* owner)
{
	if (pending_.oneShotAnimationKey.empty()) return nullptr;

	const auto& table = owner->GetData().oneShotAnimations;
	const auto it = table.find(pending_.oneShotAnimationKey);
	if (it != table.end()) return &it->second;

	// 未定義キーは捨てる(残すと以降の低優先の要求が全て弾かれるため)
	ClearPending(owner);
	return nullptr;
}

void StandardEnemyBehavior::ClearPending(EnemyAIController* owner)
{
	pending_ = PendingReaction();
	owner->GetBlackboard().SetBool(EnemyBlackboardKeys::HasPendingReaction, false);
}

bool StandardEnemyBehavior::PlayOneShotAnimation(EnemyAIController* owner, const std::string& key, bool loop)
{
	const auto& table = owner->GetData().oneShotAnimations;
	const auto it = table.find(key);
	if (it == table.end()) return false;

	if (loop) {
		owner->PlayAnimation(it->second.animationName, true);
	}
	else {
		owner->PlayAnimation(it->second.animationName, false, it->second.duration, it->second.useRootMotion);
	}
	return true;
}
