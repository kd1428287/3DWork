#include "WarrockActions.h"
#include "../EnemyAIController.h"

// --- WarrockActionIdle ---
BTNodeStatus WarrockActionIdle::Tick(EnemyAIController* context, float /*deltaTime*/)
{
	context->StopMovement();
	context->PlayAnimationIfChanged("Idle", true);
	return BTNodeStatus::Running;
}

// --- WarrockActionChase ---
BTNodeStatus WarrockActionChase::Tick(EnemyAIController* context, float /*deltaTime*/)
{
	if (!context->HasTarget()) return BTNodeStatus::Failure;

	Math::Vector3 toTarget = context->GetTargetPositionOrSelf() - context->GetPosition();
	toTarget.y = 0.0f;

	if (toTarget.LengthSquared() < 1e-6f) {
		context->StopMovement();
		return BTNodeStatus::Running;
	}

	toTarget.Normalize();
	context->SetDesiredVelocity(toTarget * context->GetData().chaseSpeed);
	context->PlayAnimationIfChanged("Run", true);
	return BTNodeStatus::Running;
}
