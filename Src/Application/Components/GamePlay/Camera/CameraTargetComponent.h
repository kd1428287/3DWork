#pragma once

// カメラの追従対象になれるようにするコンポーネント
class CameraTargetComponent : public ComponentBase, public ICameraTarget {
public:
	explicit CameraTargetComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	Math::Vector3 GetTargetPosition() const override {
		return transform_ ? (transform_->GetPosition() + localPosition_) : Math::Vector3::Zero;
	}

	Math::Quaternion GetTargetRotation() const override {
		return transform_ ? transform_->GetRotation() : Math::Quaternion::Identity;
	}

	void SetLocalPosition(Math::Vector3 position) { localPosition_ = position; }

private:
	Math::Vector3 localPosition_{};
	TransformComponent* transform_ = nullptr;
};
