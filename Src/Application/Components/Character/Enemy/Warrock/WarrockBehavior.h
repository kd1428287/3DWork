#pragma once
#include "../IEnemyBehavior.h"
#include "WarrockActions.h"

class WarrockBehavior : public IEnemyBehavior
{
public:
	std::unique_ptr<IBTNode<EnemyAIController>> BuildTree(EnemyAIController* owner) override;
	void OnSpawned(EnemyAIController* owner) override;
	void OnHit(EnemyAIController* owner, const AttackSourceComponent& attack) override;
	void OnParried(EnemyAIController* owner, const AttackSourceComponent::ParriedEvent& event) override;
	void OnDied(EnemyAIController* owner) override;
	float GetDespawnDelay() const override;

private:
	bool hitReactionPending_ = false;
	bool roarPending_ = false;
	bool bigStaggerPending_ = false;
	bool parriedPending_ = false;
};