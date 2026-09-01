#include "BruteBehavior.h"
#include "../../BehaviorTree/BTWeightedAttackAction.h"
#include "../EnemyAIController.h"
#include "../../BehaviorTree/BTComposite.h"
#include "../../BehaviorTree/BTCondition.h"
#include "../../Data/PostureComponent.h"
#include "../../../Collision/AttackSourceComponent.h"

std::unique_ptr<IBTNode<EnemyAIController>> BruteBehavior::BuildTree(EnemyAIController* /*owner*/)
{
	auto attackSeq = std::make_unique<BTSequence<EnemyAIController>>();
	attackSeq->AddChild(std::make_unique<BTCondition<EnemyAIController>>(
		[](EnemyAIController* c) { return c->IsTargetInAttackRange(); }));
	attackSeq->AddChild(std::make_unique<BTWeightedAttackAction<EnemyAIController>>());

	auto chaseSeq = std::make_unique<BTSequence<EnemyAIController>>();
	chaseSeq->AddChild(std::make_unique<BTCondition<EnemyAIController>>(
		[](EnemyAIController* c) { return c->HasTarget(); }));
	chaseSeq->AddChild(std::make_unique<EnemyActionChase>());

	// 待機→巡回のループ。EnemyActionIdleが指定秒数Runningを返し続け、
	// Successで初めてEnemyActionPatrolへ進む(BTSequenceのresumption動作)。
	auto patrolSeq = std::make_unique<BTSequence<EnemyAIController>>();
	patrolSeq->AddChild(std::make_unique<EnemyActionIdle>());
	patrolSeq->AddChild(std::make_unique<EnemyActionPatrol>());

	// 優先度順(攻撃 > 追跡 > 待機/巡回)。reactive版BTSelectorのため、
	// 毎フレーム必ず攻撃条件から評価し直す。
	auto selector = std::make_unique<BTSelector<EnemyAIController>>();
	selector->AddChild(std::move(attackSeq));
	selector->AddChild(std::move(chaseSeq));
	selector->AddChild(std::move(patrolSeq));

	return selector;
}

void BruteBehavior::OnHit(EnemyAIController* owner, const AttackSourceComponent& attack)
{
	if (PostureComponent* posture = owner->GetPostureComponent()) {
		posture->AddPostureDamage(attack.postureDamage);
		if (posture->IsBroken()) {
			// TODO: 崩し状態への反応(専用の被弾リアクション)は
			// スコープ外のため未実装。
		}
	}
}
