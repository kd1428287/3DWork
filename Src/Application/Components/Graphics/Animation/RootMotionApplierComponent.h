#pragma once
#include "ModelAnimatorComponent.h"

// ModelAnimatorComponentが抽出したルートモーションを、所有者のTransformへ反映する
class RootMotionApplierComponent : public ComponentBase
{
public:
	explicit RootMotionApplierComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override
	{
		modelAnimator_ = GetOwner()->GetComponent<ModelAnimatorComponent>();
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	void Update(float /*deltaTime*/) override
	{
		if (modelAnimator_ == nullptr || transform_ == nullptr) return;

		// 並進: ローカルの移動量を現在の向きでワールドへ変換して加算する
		const Math::Vector3 localDelta = modelAnimator_->ConsumeRootMotionDelta();
		if (localDelta.LengthSquared() > 0.0f) {
			const Math::Vector3 worldDelta = Math::Vector3::Transform(localDelta, transform_->GetRotation());
			transform_->Translate(worldDelta);
		}

		// 回転: Yaw差分を現在の向きへの追加回転として右から合成する
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