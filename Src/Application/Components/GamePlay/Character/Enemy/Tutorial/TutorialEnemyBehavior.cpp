#include "TutorialEnemyBehavior.h"
#include "../EnemyAIController.h"
#include "../EnemyActions.h"
#include "Application/Definitions/Character/Enemy/EnemyBlackboardKeys.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTComposite.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTCondition.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTCommittedSelector.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTBlackboardConditions.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTPendingReactionAction.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTWeightedAttackAction.h"
#include "../../Common/AttackSourceComponent.h"

// 仮のフェーズ表。技IDは実際のJSONのidに合わせて差し替えること。
std::vector<TutorialPhase> TutorialEnemyBehavior::DefaultPhases()
{
	return {
		{ TutorialPhaseMode::Idle },                          // 0: 棒立ち(被弾リアクションの練習)
		{ TutorialPhaseMode::Scripted, "Swing", true },       // 1: 同じ技を繰り返す(回避/ガードの練習)
		{ TutorialPhaseMode::Scripted, "Thrust", false },     // 2: 1回だけ(パリィの練習)
		{ TutorialPhaseMode::Free },                          // 3: 通常戦闘
	};
}

const TutorialPhase& TutorialEnemyBehavior::CurrentPhase(EnemyAIController* c)
{
	static const TutorialPhase kIdlePhase;

	const float raw = c->GetBlackboard().GetFloatOr(kPhaseKey, 0.0f);
	const int index = static_cast<int>(raw);
	if (index != lastPhaseIndex_) {
		lastPhaseIndex_ = index;
		consumed_ = false;
	}
	if (index < 0 || index >= static_cast<int>(phases_.size())) return kIdlePhase;
	return phases_[index];
}

bool TutorialEnemyBehavior::IsScriptedPending(EnemyAIController* c)
{
	const TutorialPhase& phase = CurrentPhase(c);
	if (phase.mode != TutorialPhaseMode::Scripted) return false;
	return phase.repeat || !consumed_;
}

const EnemyAttackDefinition* TutorialEnemyBehavior::ScriptedAttack(EnemyAIController* c)
{
	const TutorialPhase& phase = CurrentPhase(c);
	if (phase.mode != TutorialPhaseMode::Scripted) return nullptr;
	return c->GetData().FindAttack(phase.attackId);
}

bool TutorialEnemyBehavior::IsInScriptedRange(EnemyAIController* c)
{
	const EnemyAttackDefinition* atk = ScriptedAttack(c);
	if (atk == nullptr || !c->HasTarget()) return false;
	const float dist = c->DistanceToTarget();
	return dist >= atk->minRange && dist <= atk->maxRange;
}

std::unique_ptr<IBTNode<EnemyAIController>> TutorialEnemyBehavior::BuildTree(EnemyAIController* /*owner*/)
{
	using Cond = BTCondition<EnemyAIController>;
	using Seq = BTSequence<EnemyAIController>;

	// 被弾リアクション(WarrockBehaviorと同じ構成)。
	auto reactionSeq = std::make_unique<Seq>();
	reactionSeq->AddChild(std::make_unique<BTCompareBoolCondition<EnemyAIController>>(
		EnemyBlackboardKeys::HasPendingReaction, true));
	reactionSeq->AddChild(std::make_unique<BTPendingReactionAction<EnemyAIController>>(
		[this](EnemyAIController* c) -> const MotionClipData* {
			if (pending_.oneShotAnimationKey.empty()) return nullptr;
			const auto& table = c->GetData().oneShotAnimations;
			const auto it = table.find(pending_.oneShotAnimationKey);
			if (it != table.end()) return &it->second;
			// キー未定義の要求は捨てる(残すと低優先度の要求が全て弾かれる)
			pending_ = PendingReaction();
			c->GetBlackboard().SetBool(EnemyBlackboardKeys::HasPendingReaction, false);
			return nullptr;
		},
		[this](EnemyAIController* c) {
			pending_ = PendingReaction();
			c->GetBlackboard().SetBool(EnemyBlackboardKeys::HasPendingReaction, false);
		}));

	// Idleフェーズ: その場で待機するだけ(被弾リアクションは上で割り込める)。
	auto idleModeSeq = std::make_unique<Seq>();
	idleModeSeq->AddChild(std::make_unique<Cond>(
		[this](EnemyAIController* c) { return IsMode(c, TutorialPhaseMode::Idle); }));
	idleModeSeq->AddChild(std::make_unique<EnemyActionIdle>());

	// Scriptedフェーズ: 指定技の間合いに入っていれば撃つ。開始したら完了まで遮られない。
	auto scriptedAttackSeq = std::make_unique<Seq>();
	scriptedAttackSeq->AddChild(std::make_unique<Cond>(
		[this](EnemyAIController* c) {
			return IsScriptedPending(c) && IsInScriptedRange(c) && !c->IsAttackOnCooldown();
		}));
	scriptedAttackSeq->AddChild(std::make_unique<BTWeightedAttackAction<EnemyAIController>>(
		[this](EnemyAIController* c) -> const EnemyAttackDefinition* {
			const EnemyAttackDefinition* atk = ScriptedAttack(c);
			if (atk != nullptr) consumed_ = true; // 選んだ時点で消化(1回限りのフェーズ用)
			return atk;
		}));
	auto scriptedAttackGroup = std::make_unique<BTCommittedSelector<EnemyAIController>>();
	scriptedAttackGroup->AddChild(std::move(scriptedAttackSeq));

	// Scriptedフェーズで間合い外なら近づく。
	auto scriptedChaseSeq = std::make_unique<Seq>();
	scriptedChaseSeq->AddChild(std::make_unique<Cond>(
		[this](EnemyAIController* c) {
			return IsScriptedPending(c) && c->HasTarget() && !IsInScriptedRange(c);
		}));
	scriptedChaseSeq->AddChild(std::make_unique<EnemyActionChase>());

	// Scriptedフェーズでクールダウン中/消化済みなら待機。
	auto scriptedWaitSeq = std::make_unique<Seq>();
	scriptedWaitSeq->AddChild(std::make_unique<Cond>(
		[this](EnemyAIController* c) { return IsMode(c, TutorialPhaseMode::Scripted); }));
	scriptedWaitSeq->AddChild(std::make_unique<EnemyActionIdle>());

	// Freeフェーズ: 通常の抽選戦闘(Warrockの攻撃/追跡と同じ構成)。
	auto freeAttackSeq = std::make_unique<Seq>();
	freeAttackSeq->AddChild(std::make_unique<Cond>(
		[this](EnemyAIController* c) { return IsMode(c, TutorialPhaseMode::Free); }));
	freeAttackSeq->AddChild(std::make_unique<BTCompareBoolCondition<EnemyAIController>>(
		EnemyBlackboardKeys::IsTargetInAttackRange, true));
	freeAttackSeq->AddChild(std::make_unique<BTCompareBoolCondition<EnemyAIController>>(
		EnemyBlackboardKeys::IsAttackOnCooldown, false));
	freeAttackSeq->AddChild(std::make_unique<BTWeightedAttackAction<EnemyAIController>>(
		[](EnemyAIController* c) { return c->ChooseAttack(); }));
	auto freeAttackGroup = std::make_unique<BTCommittedSelector<EnemyAIController>>();
	freeAttackGroup->AddChild(std::move(freeAttackSeq));

	auto freeChaseSeq = std::make_unique<Seq>();
	freeChaseSeq->AddChild(std::make_unique<Cond>(
		[this](EnemyAIController* c) { return IsMode(c, TutorialPhaseMode::Free) && c->HasTarget(); }));
	freeChaseSeq->AddChild(std::make_unique<EnemyActionMaintainDistance>());

	// 優先度順。
	auto selector = std::make_unique<BTSelector<EnemyAIController>>();
	selector->AddChild(std::move(reactionSeq));
	selector->AddChild(std::move(idleModeSeq));
	selector->AddChild(std::move(scriptedAttackGroup));
	selector->AddChild(std::move(scriptedChaseSeq));
	selector->AddChild(std::move(scriptedWaitSeq));
	selector->AddChild(std::move(freeAttackGroup));
	selector->AddChild(std::move(freeChaseSeq));
	selector->AddChild(std::make_unique<EnemyActionIdle>());

	return selector;
}

void TutorialEnemyBehavior::OnStaggered(EnemyAIController* owner, bool isLarge, float /*duration*/)
{
	RequestReaction(owner, isLarge ? "BigStagger" : "HitReaction",
		isLarge ? ReactionPriority::BigStagger : ReactionPriority::HitReaction);
}

void TutorialEnemyBehavior::OnParrySucceeded(EnemyAIController* owner, const Events::ParrySucceeded& event)
{
	RequestReaction(owner, "ParrySucceeded", ReactionPriority::Parried);

	if (PostureComponent* posture = owner->GetPostureComponent()) {
		posture->AddPostureDamage(event.parryPostureDamage);
		if (posture->IsBroken()) {
			posture->Reset();
			RequestReaction(owner, "BigStagger", ReactionPriority::BigStagger);
		}
	}
}

void TutorialEnemyBehavior::OnDied(EnemyAIController* owner)
{
	owner->StopMovement();

	const auto& table = owner->GetData().oneShotAnimations;
	const auto it = table.find("Dying");
	if (it != table.end()) {
		owner->PlayAnimation(it->second.animationName, false, it->second.duration, it->second.useRootMotion);
	}
}

float TutorialEnemyBehavior::GetDespawnDelay(const EnemyAIController* owner) const
{
	const auto& data = owner->GetData();
	const auto it = data.oneShotAnimations.find("Dying");
	const float dyingDuration = (it != data.oneShotAnimations.end()) ? it->second.duration : 0.0f;
	return dyingDuration + data.postDeathLingerSeconds;
}

void TutorialEnemyBehavior::RequestReaction(EnemyAIController* owner, const std::string& oneShotAnimationKey, ReactionPriority priority)
{
	if (pending_.oneShotAnimationKey.empty() || priority < pending_.priority) {
		pending_ = { oneShotAnimationKey, priority };
		if (owner != nullptr) {
			owner->GetBlackboard().SetBool(EnemyBlackboardKeys::HasPendingReaction, true);
		}
	}
}
