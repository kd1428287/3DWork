#pragma once
#include "Application/Definitions/Character/Common/BehaviorTree/IBTNode.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTNodeStatus.h"

class EnemyAIController;

class WarrockActionIdle : public IBTNode<EnemyAIController>
{
public:
	BTNodeStatus Tick(EnemyAIController* context, float deltaTime) override;
};

class WarrockActionChase : public IBTNode<EnemyAIController>
{
public:
	BTNodeStatus Tick(EnemyAIController* context, float deltaTime) override;
};
