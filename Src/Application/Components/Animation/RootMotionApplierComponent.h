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
		if (localDelta.LengthSquared() > 0.0f) {
			const Math::Vector3 worldDelta = Math::Vector3::Transform(localDelta, transform_->GetRotation());
			transform_->Translate(worldDelta);
		}

		// 回転も並進と同じ考え方: ローカル空間で測ったYaw差分を、現在の向きに
		// 対する「さらなる回転」として合成する(conjugateではなく右からの
		// 合成でよい。並進をtransform_->GetRotation()で world へ変換している
		// のと役割は同じで、回転の場合は右からの掛け算がそのまま相当する)。
		const float yawDelta = modelAnimator_->ConsumeRootMotionYawDelta();
		if (yawDelta != 0.0f) {
			const Math::Quaternion deltaRot = Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yawDelta);
			transform_->SetRotation(transform_->GetRotation() * deltaRot);
		}
	}

private:
	ModelAnimatorComponent* modelAnimator_ = nullptr; 
	TransformComponent* transform_ = nullptr;    
};
