#pragma once

#include "../../Physics/Movement/MovementComponent.h"
#include "../../Physics/Movement/VelocityComponent.h"

// 初期設定(Prefab/JSONから渡す値)。意味は下のコンストラクタ参照。
struct FacingDirectionConfig
{
	float rotationSpeed = 10.0f;
	float moveThreshold = 0.001f;
};

class FacingDirectionComponent : public ComponentBase {
public:
	using Config = FacingDirectionConfig;

	explicit FacingDirectionComponent(GameObject* owner,
		float rotationSpeed = FacingDirectionConfig{}.rotationSpeed,
		float moveThreshold = FacingDirectionConfig{}.moveThreshold)
		: ComponentBase(owner), rotationSpeed_(rotationSpeed), moveThreshold_(moveThreshold) {}

	void SetConfig(const Config& config)
	{
		rotationSpeed_ = config.rotationSpeed;
		moveThreshold_ = config.moveThreshold;
	}

	void Awake() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		movement_ = GetOwner()->GetComponent<MovementComponent>();
		// velocity_はIsImpulseActive()判定を廃止したため不要(前回の変更で削除済み)
	}

	void PostUpdate(float deltaTime) override
	{
		if (!updateEnabled_) return;
		if (transform_ == nullptr) return;

		Math::Vector3 dir;
		if (hasTargetOverride_) {
			// 外部(攻撃対象への正対等)から明示された方向を最優先する
			dir = targetOverrideDirection_;
		}
		else {
			if (movement_ == nullptr) return;
			dir = movement_->GetDesiredDirection();
			if (dir == Math::Vector3::Zero) return;
		}

		lastMoveDirection_ = dir;
		const Math::Quaternion targetRotation = LookRotationYawOnly(dir);

		const float t = std::clamp(rotationSpeed_ * deltaTime, 0.0f, 1.0f);
		transform_->SetRotation(Math::Quaternion::Slerp(transform_->GetRotation(), targetRotation, t));
	}

	void SetRotationSpeed(float speed) { rotationSpeed_ = speed; }
	float GetRotationSpeed() const { return rotationSpeed_; }

	void SetMoveThreshold(float threshold) { moveThreshold_ = threshold; }
	float GetMoveThreshold() const { return moveThreshold_; }

	void SetUpdateEnabled(bool enabled) { updateEnabled_ = enabled; }
	bool IsUpdateEnabled() const { return updateEnabled_; }

	// 移動方向への自動追従より優先される、外部から明示した水平方向へ向く。
	// PlayerFacingComponent::FaceTowards(攻撃対象/ロック対象への正対)から使う。
	void SetTargetOverrideDirection(const Math::Vector3& horizontalDir)
	{
		if (horizontalDir.LengthSquared() <= kEpsilon) return;
		targetOverrideDirection_ = horizontalDir;
		targetOverrideDirection_.Normalize();
		hasTargetOverride_ = true;
	}

	// override解除。移動方向への自動追従に戻す。
	void ClearTargetOverrideDirection() { hasTargetOverride_ = false; }

private:
	// Math::Vector3::Forward(TransformComponent::GetForward()が基準にしている
	// ローカル前方軸)から、水平方向dirへの最短回転を内積・外積だけで求める。
	// atan2ベースの角度計算を使わないのは、エンジンのForward軸が+Z/-Zの
	// どちらの規約でも(左手系/右手系のどちらでも)同じコードで正しく
	// 動くようにするため。
	static Math::Quaternion LookRotationYawOnly(const Math::Vector3& dir) {
		constexpr float kEpsilon = 1e-5f;

		const Math::Vector3 from = Math::Vector3::Forward;
		const float dot = std::clamp(from.Dot(dir), -1.0f, 1.0f);

		if (dot > 1.0f - kEpsilon) return Math::Quaternion::Identity; // 既に正面を向いている

		if (dot < -1.0f + kEpsilon) {
			// ほぼ真後ろ: 外積がゼロベクトルに近づき回転軸が定まらないため、
			// Up軸まわり180度の回転で代用する。
			const float kPi = std::acos(-1.0f);
			return Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, kPi);
		}

		Math::Vector3 axis = from.Cross(dir);
		axis.Normalize();
		const float angle = std::acos(dot);
		return Math::Quaternion::CreateFromAxisAngle(axis, angle);
	}

	TransformComponent* transform_ = nullptr;
	MovementComponent* movement_ = nullptr;

	Math::Vector3 lastMoveDirection_{};

	bool hasTargetOverride_ = false;
	Math::Vector3 targetOverrideDirection_{};

	float rotationSpeed_;
	float moveThreshold_;
	bool updateEnabled_ = true;

	static constexpr float kEpsilon = 1e-5f;
};