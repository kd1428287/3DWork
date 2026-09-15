#include "PlayerCombatMovementComponent.h"

#include "../../../Physics/Movement/MovementComponent.h"
#include "../../../Physics/Movement/TweenMoveComponent.h"

void PlayerCombatMovementComponent::Start()
{
	movementComponent_ = GetOwner()->GetComponent<MovementComponent>();
}

void PlayerCombatMovementComponent::ApplyMovementState(MovementState state)
{
	if (movementComponent_ == nullptr) return;
	switch (state) {
	case MovementState::Stand: break;
	case MovementState::Walk: movementComponent_->SetSpeed(walkSpeed_); break;
	case MovementState::Run:  movementComponent_->SetSpeed(runSpeed_); break;
	}
}

void PlayerCombatMovementComponent::UpdateMovementState(MovementState state, float /*deltaTime*/)
{
	if (state == MovementState::Run) {
		// スタミナ消費処理用スペース
	}
}

void PlayerCombatMovementComponent::SetMovementEnabled(bool enabled)
{
	if (movementComponent_ != nullptr) {
		movementComponent_->SetEnabled(enabled);
	}
}

void PlayerCombatMovementComponent::RequestStepMove(const Math::Vector3& direction, float distance, float duration)
{
	GameObject* owner = GetOwner();
	TransformComponent* transform = owner->GetComponent<TransformComponent>();
	if (transform == nullptr) return;

	Math::Vector3 dir = direction;
	if (dir.LengthSquared() <= kDirectionEpsilon) {
		dir = transform->GetForward();
	}

	const Math::Vector3 from = transform->GetPosition();
	const Math::Vector3 to = from + dir * distance;
	owner->RequestAddComponent<TweenMoveComponent>(from, to, duration);
}

void PlayerCombatMovementComponent::RequestStepMoveTowardsTarget(GameObject* target,
	const Math::Vector3& fallbackDirection, float stepDistance, float engageDistance, float duration)
{
	if (target == nullptr) {
		RequestStepMove(fallbackDirection, stepDistance, duration);
		return;
	}

	TransformComponent* targetTransform = target->GetComponent<TransformComponent>();
	TransformComponent* transform = GetOwner()->GetComponent<TransformComponent>();
	if (targetTransform == nullptr || transform == nullptr) {
		RequestStepMove(fallbackDirection, stepDistance, duration);
		return;
	}

	Math::Vector3 toTarget = targetTransform->GetPosition() - transform->GetPosition();
	toTarget.y = 0.0f;
	const float distanceToTarget = toTarget.Length() - 1; // 敵の大きさが読めるようになったら入れ替え

	const float closingDistance = std::min(stepDistance, std::max(0.0f, distanceToTarget - engageDistance));

	if (closingDistance <= kDirectionEpsilon) {
		return;
	}

	Math::Vector3 dir = toTarget;
	if (dir.LengthSquared() <= kDirectionEpsilon) {
		dir = transform->GetForward();
	}
	else {
		dir.Normalize();
	}

	const Math::Vector3 from = transform->GetPosition();
	const Math::Vector3 to = from + dir * closingDistance;
	GetOwner()->RequestAddComponent<TweenMoveComponent>(from, to, duration);
}

void PlayerCombatMovementComponent::CancelStepMove()
{
	GetOwner()->RequestRemoveComponent<TweenMoveComponent>();
}
