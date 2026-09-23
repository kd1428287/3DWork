#include "PlayerCombatMovementComponent.h"

#include "../../../Physics/Movement/MovementComponent.h"
#include "../../../Physics/Movement/TweenMoveComponent.h"

void PlayerCombatMovementComponent::Awake()
{
	movementComponent_ = GetOwner()->GetComponent<MovementComponent>();

	// 以前はRequestStepMove()の都度RequestAddComponentしていたが、
	// 生成時に一度だけアタッチし、以降はenabled_フラグで使い回す方式に変更。
	// ※ AddComponent<T>()がこの環境の「即時追加」APIである前提で書いている。
	//   環境側のAPI名がRequestAddComponent<T>()しか無い(Start()時点でも
	//   即時追加ができない)場合は、そちらに置き換えてください。
	tweenMoveComponent_ = GetOwner()->GetComponent<TweenMoveComponent>();
	if (tweenMoveComponent_ == nullptr) {
		tweenMoveComponent_ = GetOwner()->RequestAddComponent<TweenMoveComponent>();
	}
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
	if (transform == nullptr || tweenMoveComponent_ == nullptr) return;

	Math::Vector3 dir = direction;
	if (dir.LengthSquared() <= kDirectionEpsilon) {
		dir = transform->GetForward();
	}

	const Math::Vector3 from = transform->GetPosition();
	const Math::Vector3 to = from + dir * distance;
	tweenMoveComponent_->Play(from, to, duration);
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
	if (targetTransform == nullptr || transform == nullptr || tweenMoveComponent_ == nullptr) {
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
	tweenMoveComponent_->Play(from, to, duration);
}

void PlayerCombatMovementComponent::CancelStepMove()
{
	if (tweenMoveComponent_ != nullptr) {
		tweenMoveComponent_->SetEnabled(false);
	}
}