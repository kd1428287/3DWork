#include "EnemyActions.h"
#include "EnemyAIController.h"

// --- EnemyActionIdle ---
BTNodeStatus EnemyActionIdle::Tick(EnemyAIController* context, float deltaTime)
{
	context->StopMovement();
	context->PlayAnimationIfChanged("Idle", true);

	// 巡回地点が無ければ巡回へ進む意味が無いため、待機のまま居座る。
	if (context->GetData().patrolPoints.empty()) {
		return BTNodeStatus::Running;
	}

	elapsed_ += deltaTime;
	if (elapsed_ >= context->GetData().idleDuration) {
		elapsed_ = 0.0f;
		return BTNodeStatus::Success;
	}
	return BTNodeStatus::Running;
}

// --- EnemyActionPatrol ---
BTNodeStatus EnemyActionPatrol::Tick(EnemyAIController* context, float /*deltaTime*/)
{
	if (context->GetData().patrolPoints.empty()) return BTNodeStatus::Failure;

	const Math::Vector3 target = context->GetCurrentPatrolPoint();
	Math::Vector3 toTarget = target - context->GetPosition();
	toTarget.y = 0.0f;

	if (toTarget.LengthSquared() <= kArriveThreshold * kArriveThreshold) {
		context->StopMovement();
		context->AdvanceToNextPatrolPoint();
		return BTNodeStatus::Success;
	}

	toTarget.Normalize();
	context->SetDesiredVelocity(toTarget * context->GetData().patrolSpeed);
	context->PlayAnimationIfChanged("Walk", true);
	return BTNodeStatus::Running;
}

// --- EnemyActionChase ---
BTNodeStatus EnemyActionChase::Tick(EnemyAIController* context, float /*deltaTime*/)
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

// --- EnemyActionMaintainDistance ---
BTNodeStatus EnemyActionMaintainDistance::Tick(EnemyAIController* context, float /*deltaTime*/)
{
	if (!context->HasTarget()) return BTNodeStatus::Failure;

	Math::Vector3 toTarget = context->GetTargetPositionOrSelf() - context->GetPosition();
	toTarget.y = 0.0f;

	const float maintainDistance = context->GetData().maintainDistance;

	// 間合い(maintainDistance)以下まで来たら、それ以上は接近せずその場で
	// 停止する(近すぎても後退はしない設計。EnemyActions.h冒頭コメント参照)。
	if (toTarget.LengthSquared() <= maintainDistance * maintainDistance) {
		context->StopMovement();
		context->PlayAnimationIfChanged("Idle", true);
		return BTNodeStatus::Running;
	}

	toTarget.Normalize();
	context->SetDesiredVelocity(toTarget * context->GetData().chaseSpeed);
	context->PlayAnimationIfChanged("Run", true);
	return BTNodeStatus::Running;
}