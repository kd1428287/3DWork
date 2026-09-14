#pragma once

#include "MovementComponent.h"
#include "VelocityComponent.h"

// 外力や内力を合成する
class MotionComposerComponent : public ComponentBase
{
public:
	explicit MotionComposerComponent(GameObject* owner) :ComponentBase(owner) {};

	void Start() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		movement_ = GetOwner()->GetComponent<MovementComponent>();
		velocity_ = GetOwner()->GetComponent<VelocityComponent>();
	}

	void Update(float deltaTime) override
	{
		if (!transform_)return;
		const float clampedDeltaTime = std::min(deltaTime, kMaxDeltaTime);

		if (movement_)
		{
			movement_->AcumulateDesiredVelocity();
		}
		if (velocity_)
		{
			velocity_->DampingUpdate(clampedDeltaTime);
		}

	}

	void PostUpdate(float deltaTime) override
	{
		if (!transform_)return;
		const float clampedDeltaTime = std::min(deltaTime, kMaxDeltaTime);

		Math::Vector3 eV = !velocity_ ? Math::Vector3::Zero : velocity_->GetVelocity();
		Math::Vector3 iV = !movement_ ? Math::Vector3::Zero : movement_->GetVelocity();

		Math::Vector3 composedVelocity = eV + iV;
		if (composedVelocity.LengthSquared() > kMaxSpeed * kMaxSpeed)
		{
			composedVelocity.Normalize();
			composedVelocity *= kMaxSpeed;
		}

		composedVelocity *= clampedDeltaTime;
		transform_->SetPosition(transform_->GetPosition() + composedVelocity);
	}

private:
	TransformComponent* transform_ = nullptr;
	MovementComponent* movement_ = nullptr;
	VelocityComponent* velocity_ = nullptr;

	static constexpr float kMaxDeltaTime = 1.0f / 30.0f;
	static constexpr float kMaxSpeed = 25.0f;
};