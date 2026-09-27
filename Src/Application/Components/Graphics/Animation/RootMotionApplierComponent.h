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

		// 並進: ローカルの移動量にワープ補正(Yaw回転・距離スケール)をかけてから、
		// 現在の向きでワールドへ変換して加算する。warpRotation_/warpScale_が
		// 既定値(Identity/1.0)なら、従来通り素のルートモーションと同じ結果になる。
		const Math::Vector3 localDelta = modelAnimator_->ConsumeRootMotionDelta();
		if (localDelta.LengthSquared() > 0.0f) {
			const Math::Vector3 warpedLocal = Math::Vector3::Transform(localDelta, warpRotation_) * warpScale_;
			const Math::Vector3 worldDelta = Math::Vector3::Transform(warpedLocal, transform_->GetRotation());
			transform_->Translate(worldDelta);
		}

		// 回転: Yaw差分を現在の向きへの追加回転として右から合成する
		const float yawDelta = modelAnimator_->ConsumeRootMotionYawDelta();
		if (yawDelta != 0.0f) {
			const Math::Quaternion deltaRot = Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yawDelta);
			transform_->SetRotation(transform_->GetRotation() * deltaRot);
		}
	}

	// 以降のルートモーション移動量に補正をかける(モーションワーピング)。
	// warpRotationはボーンローカル空間での追加Yaw回転、warpScaleは距離の倍率。
	// 敵への踏み込み等、狙った位置へ届かせたい区間の開始時に呼ぶ想定
	// (PlayerCombatMovementComponent::RequestRootMotionWarpTowardsTarget参照)。
	void SetRootMotionWarp(const Math::Quaternion& warpRotation, float warpScale)
	{
		warpRotation_ = warpRotation;
		warpScale_ = warpScale;
	}

	// 補正を解除し、素のルートモーションへ戻す(区間終了時に呼ぶ)。
	void ClearRootMotionWarp()
	{
		warpRotation_ = Math::Quaternion::Identity;
		warpScale_ = 1.0f;
	}

private:
	ModelAnimatorComponent* modelAnimator_ = nullptr;
	TransformComponent* transform_ = nullptr;

	Math::Quaternion warpRotation_ = Math::Quaternion::Identity;
	float warpScale_ = 1.0f;
};