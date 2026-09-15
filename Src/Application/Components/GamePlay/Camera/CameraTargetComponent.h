#pragma once

// カメラの追従対象になれるようにするコンポーネント
class CameraTargetComponent : public ComponentBase, public ICameraTarget {
public:
	explicit CameraTargetComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	Math::Vector3 GetFixationPoint() const override {
		return transform_ ? (transform_->GetPosition() + offset_) : Math::Vector3::Zero;
	}

	Math::Quaternion GetTargetRotation() const override {
		return transform_ ? transform_->GetRotation() : Math::Quaternion::Identity;
	}

	void SetOffset(const Math::Vector3& offset) { offset_ = offset; }

private:
	Math::Vector3 offset_{};
	TransformComponent* transform_ = nullptr;
};
