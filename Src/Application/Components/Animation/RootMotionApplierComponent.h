#pragma once
#include "ModelAnimatorComponent.h"
#include "../Transform/TransformComponent.h"

class RootMotionApplierComponent : public ComponentBase
{
public:
	explicit RootMotionApplierComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override
	{
		modelAnimator_ = GetOwner()->GetComponent<ModelAnimatorComponent>();
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	void Update(float /*deltaTime*/) override
	{
		if (modelAnimator_ == nullptr || transform_ == nullptr) return;

		const Math::Vector3 localDelta = modelAnimator_->ConsumeRootMotionDelta();
		if (localDelta.LengthSquared() <= 0.0f) return;

		const Math::Vector3 worldDelta = Math::Vector3::Transform(localDelta, transform_->GetRotation());
		transform_->Translate(worldDelta);
	}

private:
	ModelAnimatorComponent* modelAnimator_ = nullptr; 
	TransformComponent* transform_ = nullptr;    
};
