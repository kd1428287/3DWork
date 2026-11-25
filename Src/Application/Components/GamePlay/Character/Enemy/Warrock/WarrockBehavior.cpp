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

// --- IEnemyBehavior -------------------------------------------------------
void WarrockBehavior::OnSpawned(EnemyAIController* owner)
{
	RequestReaction(owner, "Roar", ReactionPriority::Spawn);
	owner->PlaySE(owner->GetSoundData().roar);
}
