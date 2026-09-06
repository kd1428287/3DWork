// PlayerStatusController.cpp
#include "PlayerStatusController.h"
#include "../../Movement/TweenMoveComponent.h"
#include <algorithm> // std::min/std::max (RequestStepMoveTowardsTarget)

void PlayerStatusController::HandleMovementInput(const PlayerInputComponent& input, float deltaTime)
{
	if (GetCombatState() != CombatState::None) return;
	const MovementState nextState = input.GetDesiredMovementState();

	if (movementState_ != nextState) {
		movementState_ = nextState;
		ApplyMovementState(movementState_);
	}

	if (facingDirectionComponent_ != nullptr) {
		const bool shouldFaceMovement = !IsLockedOn() || movementState_ == MovementState::Run;
		facingDirectionComponent_->SetUpdateEnabled(shouldFaceMovement);
	}

	if (movementAnimationComponent_ != nullptr) {
		movementAnimationComponent_->Tick(deltaTime, movementState_, input.GetMoveDirection(), IsLockedOn());
	}
}
void PlayerStatusController::HandleActionInput(PlayerInputComponent& input)
{
	// スタン中(Stagger)は一切の行動入力を受け付けない。
	if (IsStaggered()) return;

	if (input.ConsumeLockPressed()) {
		if (IsLockedOn()) {
			ClearLockOn();
		}
		else {
			TryLockOn();
		}
	}

	if (input.IsGuardHeld()) {
		TryStartGuard();
	}
	else if (GetCombatState() == CombatState::Guard) {
		ChangeStateToNone(); // ガードキーを離したら即解除
	}

	if (input.HasCommand(ActionCommand::Evade) && CanStartEvade()) {
		EvadeMoveData data = baseEvadeData_;
		input.ConsumeCommand(ActionCommand::Evade, data.evadeDirection);
		TryStartEvade(data);
	}
	else if (input.HasCommand(ActionCommand::Attack) && CanStartAttack()) {
		input.ConsumeCommand(ActionCommand::Attack);
		TryStartAttack();
	}
}

void PlayerStatusController::ApplyMovementState(MovementState state)
{
	if (!movementComponent_) return;
	switch (state) {
	case MovementState::Stand: break;
	case MovementState::Walk: movementComponent_->SetSpeed(walkSpeed_); break;
	case MovementState::Run:  movementComponent_->SetSpeed(runSpeed_); break;
	}
}

void PlayerStatusController::UpdateMovementState(float deltaTime)
{
	if (GetCombatState() != CombatState::None) return;

	if (movementState_ == MovementState::Run) {
		// スタミナ消費処理用スペース
	}
}

void PlayerStatusController::RequestStepMove(const Math::Vector3& direction, float distance, float duration)
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

void PlayerStatusController::CancelStepMove()
{
	GetOwner()->RequestRemoveComponent<TweenMoveComponent>();
}

void PlayerStatusController::RequestStepMoveTowardsTarget(const Math::Vector3& fallbackDirection, float stepDistance,
	float engageDistance, float duration)
{
	GameObject* target = currentAttackTarget_.Resolve();
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
	const float distanceToTarget = toTarget.Length();

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