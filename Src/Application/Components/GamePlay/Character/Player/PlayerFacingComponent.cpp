#include "PlayerFacingComponent.h"
#include "PlayerLockOnComponent.h"
#include "../../../Graphics/Animation/FacingDirectionComponent.h"

void PlayerFacingComponent::Start()
{
	transform_ = GetOwner()->GetComponent<TransformComponent>();
	facingDirectionComponent_ = GetOwner()->GetComponent<FacingDirectionComponent>();
	lockOnComponent_ = GetOwner()->GetComponent<PlayerLockOnComponent>();
}

void PlayerFacingComponent::FaceAttackTarget()
{
	currentAttackTarget_ = {};

	if (lockOnComponent_ == nullptr || transform_ == nullptr) return;

	GameObject* target = lockOnComponent_->IsLockedOn()
		? lockOnComponent_->GetLockedTarget()
		: lockOnComponent_->FindNearestToScreenCenter();

	currentAttackTarget_ = Handle<GameObject>(target);

	FaceTowards(target);
}

void PlayerFacingComponent::UpdateLockOnFacing(bool isRunning)
{
	if (facingDirectionComponent_ == nullptr) return;

	facingDirectionComponent_->SetUpdateEnabled(true);

	// 走行中は方向ロック解除(入力方向へ普通に正対させる。
	// PlayerMovementAnimationComponent::Tick()側のRun分岐と同じ考え方)。
	if (isRunning) return;

	const bool lockedOn = lockOnComponent_ != nullptr && lockOnComponent_->IsLockedOn();
	if (lockedOn) {
		FaceTowards(lockOnComponent_->GetLockedTarget());
	}
}

EvadeDirection PlayerFacingComponent::ClassifyEvadeDirection(const Math::Vector3& inputDirection) const
{
	// ::を付けてグローバル名前空間の自由関数(PlayerCombatTypes.h)を明示的に
	// 呼ぶ。このメンバ関数自体と同名のため、::が無いと自己再帰になる。
	const Math::Vector3 forward = (transform_ != nullptr) ? transform_->GetForward() : Math::Vector3::Zero;
	return ::ClassifyEvadeDirection(forward, inputDirection);
}

void PlayerFacingComponent::SetFacingEnabled(bool enabled)
{
	if (facingDirectionComponent_ != nullptr) {
		facingDirectionComponent_->SetUpdateEnabled(enabled);
	}
}

void PlayerFacingComponent::FaceTowards(GameObject* target)
{
	if (target == nullptr || transform_ == nullptr) return;

	TransformComponent* targetTransform = target->GetComponent<TransformComponent>();
	if (targetTransform == nullptr) return;

	Math::Vector3 dir = targetTransform->GetPosition() - transform_->GetPosition();
	dir.y = 0.0f;
	if (dir.LengthSquared() <= kDirectionEpsilon) return;
	dir.Normalize();
	dir = -dir;

	const float yaw = std::atan2(dir.x, dir.z);
	transform_->SetRotation(Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yaw));
}
