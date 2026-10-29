// PlayerStateContext.cpp
#include "PlayerStateContext.h"

#include "PlayerFacingComponent.h"
#include "PlayerCombatMovementComponent.h"
#include "PlayerLockOnComponent.h"
#include "../../../Graphics/Animation/ModelAnimatorComponent.h"
#include "Application/Core/EventBus/Events/AudioEvents.h"

bool PlayerStateContext::IsLockedOn() const
{
	return lockOn != nullptr && lockOn->IsLockedOn();
}

void PlayerStateContext::PlayAnimation(const std::string& name, bool loop, float targetDurationSeconds, float startTime, float endTime,
	bool useRootMotion, float blendDurationSeconds)
{
	if (animator == nullptr) return;
	// ボーン名はSetConfig(ModelAnimatorConfig)で設定済み。ここでは有効/無効の切り替えのみ行う。
	animator->SetRootMotionEnabled(useRootMotion);
	animator->SetBlendDuration(blendDurationSeconds);
	animator->Play(name, loop, targetDurationSeconds, startTime, endTime, true);
}

void PlayerStateContext::PlayAnimation(const std::string& name, bool loop, float targetDurationSeconds, bool useRootMotion, float blendDurationSeconds)
{
	PlayAnimation(name, loop, targetDurationSeconds, 0.0f, 0.0f, useRootMotion, blendDurationSeconds);
}

void PlayerStateContext::PlayAnimation(const MotionClipData& clip)
{
	if (animator == nullptr) return;
	animator->Play(clip);
}

void PlayerStateContext::PlaySE(const std::string& id) const
{
	if (id.empty()) return;
	Events::PublishSoundPlayEvent(*GlobalEventBus::Instance(), id);
}

void PlayerStateContext::RequestStepMoveTowardsTarget(const Math::Vector3& fallbackDirection,
	float stepDistance, float engageDistance, float duration)
{
	if (combatMovement == nullptr) return;

	GameObject* target = (facing != nullptr) ? facing->GetCurrentAttackTarget() : nullptr;
	combatMovement->RequestStepMoveTowardsTarget(target, fallbackDirection, stepDistance, engageDistance, duration);
}

void PlayerStateContext::RequestRootMotionWarpTowardsTarget(float expectedDistance, float engageDistance,
	float minScale, float maxScale)
{
	if (combatMovement == nullptr) return;

	GameObject* target = (facing != nullptr) ? facing->GetCurrentAttackTarget() : nullptr;
	combatMovement->RequestRootMotionWarpTowardsTarget(target, expectedDistance, engageDistance, minScale, maxScale);
}
