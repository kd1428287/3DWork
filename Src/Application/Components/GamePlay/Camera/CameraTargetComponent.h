#pragma once
#include "Application/Definitions/Character/Common/CharacterCollisionDefaults.h"

// 初期設定(Prefab/JSONから渡す値)。既定値は目線の高さ(体格の共通定数)。
struct CameraTargetConfig
{
	Math::Vector3 offset = { 0.0f, CharacterCollisionDefaults::kEyeHeight, 0.0f };
};

// カメラの追従対象になれるようにするコンポーネント
class CameraTargetComponent : public ComponentBase, public ICameraTarget {
public:
	using Config = CameraTargetConfig;

	explicit CameraTargetComponent(GameObject* owner) : ComponentBase(owner) {}

	void SetConfig(const Config& config) { offset_ = config.offset; }

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
	// 高いほど優先度が高い
	void SetPriority(const float priority) { priority_ = priority; }

private:
	Math::Vector3 offset_ = CameraTargetConfig{}.offset;
	float priority_ = 0.0f;
	TransformComponent* transform_ = nullptr;
};
