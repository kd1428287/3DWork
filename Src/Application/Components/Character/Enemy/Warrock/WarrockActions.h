#pragma once
#include "../../BehaviorTree/IBTNode.h"

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
