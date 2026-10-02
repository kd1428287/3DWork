#include "PlayerCombatMovementComponent.h"

#include "../../../Physics/Movement/MovementComponent.h"
#include "../../../Physics/Movement/TweenMoveComponent.h"
#include "../../../Graphics/Animation/RootMotionApplierComponent.h"
#include "../../../Physics/Collision/BodySurfaceQuery.h"

namespace
{
	// 自分と相手の胴体の表面間距離(水平)。胴体が取れなければ中心間距離で代用する。
	float ComputeSurfaceGapXZ(GameObject* self, GameObject* target, float centerDistanceXZ)
	{
		const ColliderComponent* selfCollider = self->GetComponent<ColliderComponent>();
		const ColliderComponent* targetCollider = target->GetComponent<ColliderComponent>();
		if (selfCollider != nullptr && targetCollider != nullptr) {
			if (const std::optional<float> gap = BodySurfaceQuery::GetGapXZ(*selfCollider, *targetCollider)) {
				return *gap;
			}
		}

		std::printf("[PlayerCombatMovement] warning: body shape not set (%s -> %s). using center distance\n",
			self->GetName().c_str(), target->GetName().c_str());
		return centerDistanceXZ;
	}
}

void PlayerCombatMovementComponent::Awake()
{
	movementComponent_ = GetOwner()->GetComponent<MovementComponent>();
	tweenMoveComponent_ = GetOwner()->GetComponent<TweenMoveComponent>();
	if (tweenMoveComponent_ == nullptr) {
		tweenMoveComponent_ = GetOwner()->RequestAddComponent<TweenMoveComponent>();
	}

	rootMotionApplier_ = GetOwner()->GetComponent<RootMotionApplierComponent>();
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
	// 中心ではなく、胴体の表面同士の間隔を基準に詰める距離を決める
	const float gapToTarget = ComputeSurfaceGapXZ(GetOwner(), target, toTarget.Length());

	const float closingDistance = std::min(stepDistance, std::max(0.0f, gapToTarget - engageDistance));

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

void PlayerCombatMovementComponent::RequestRootMotionWarpTowardsTarget(GameObject* target,
	float expectedDistance, float engageDistance, float minScale, float maxScale)
{
	if (rootMotionApplier_ == nullptr) return;

	TransformComponent* transform = GetOwner()->GetComponent<TransformComponent>();
	TransformComponent* targetTransform = (target != nullptr) ? target->GetComponent<TransformComponent>() : nullptr;
	if (transform == nullptr || targetTransform == nullptr || expectedDistance <= kDirectionEpsilon) {
		rootMotionApplier_->ClearRootMotionWarp(); // 対象なし: 素のルートモーションのまま
		return;
	}

	Math::Vector3 toTarget = targetTransform->GetPosition() - transform->GetPosition();
	toTarget.y = 0.0f;
	const float distanceToTarget = toTarget.Length();
	if (distanceToTarget <= kDirectionEpsilon) {
		rootMotionApplier_->ClearRootMotionWarp();
		return;
	}
	toTarget.Normalize();

	// 向き: 現在の前方からtoTargetへの水平角度差をそのままYaw補正量にする
	const float yawDiff = ComputeHorizontalAngleTo(transform->GetForward(), toTarget);
	const Math::Quaternion warpRotation = Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yawDiff);

	// 距離: 胴体の表面間隔からengageDistanceを残して詰めたい距離と、クリップが素で進む想定距離との比
	const float gapToTarget = ComputeSurfaceGapXZ(GetOwner(), target, distanceToTarget);
	const float desiredDistance = std::max(0.0f, gapToTarget - engageDistance);
	const float scale = std::clamp(desiredDistance / expectedDistance, minScale, maxScale);

	rootMotionApplier_->SetRootMotionWarp(warpRotation, scale);
}

void PlayerCombatMovementComponent::SetRootMotionScale(float scale)
{
	if (rootMotionApplier_ != nullptr) {
		rootMotionApplier_->SetRootMotionWarp(Math::Quaternion::Identity, scale);
	}
}

void PlayerCombatMovementComponent::ClearRootMotionWarp()
{
	if (rootMotionApplier_ != nullptr) {
		rootMotionApplier_->ClearRootMotionWarp();
	}
}