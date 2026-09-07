#pragma once
#include "IMovementSource.h"
#include "IRawDirectionSource.h"
#include "../Camera/CameraComponent.h"

enum class ConversionMode
{
	camera = 0,
	self,
};

class DirectionTransformSource : public ComponentBase, public IMovementSource
{
public:
	explicit DirectionTransformSource(GameObject* owner)
		: ComponentBase(owner) {}

	void Start() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	Math::Vector3 GetDesiredVelocity() override
	{
		moveDir_ = source_->GetRawDirection();
		switch (mode_)
		{
		case ConversionMode::camera:
			return ConversionByCamera(moveDir_);
		case ConversionMode::self:
			return ConversionBySelf(moveDir_);
		default:
			return Math::Vector3::Zero;
		}
	}

	Math::Vector3 GetMoveDirection() const { return moveDir_; }

	void SetMovementSource(IRawDirectionSource* source) { source_ = source; }
	void SetConversionMode(ConversionMode mode) { mode_ = mode; }
	ConversionMode GetConversionMode() { return mode_; }

private:
	Math::Vector3 ConversionByCamera(Math::Vector3& moveDirection)
	{
		Math::Vector3 moveDir = moveDirection;
		// カメラの水平方向(yaw)を移動方向の基準にする
		bool usedActualCameraForward = false;
		if (SceneContext* context = GetOwner()->GetContext()) {
			if (CameraComponent* camera = context->activeCamera) {
				Math::Vector3 camForward = camera->GetForward();
				camForward.y = 0.0f;
				if (camForward.LengthSquared() > kMinCameraForwardLengthSq) {
					camForward.Normalize();
					const float yaw = std::atan2(-camForward.x, -camForward.z);
					const Math::Quaternion yawOnly =
						Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yaw);
					moveDir = Math::Vector3::Transform(moveDir, yawOnly);
					usedActualCameraForward = true;
				}
			}
		}

		return moveDir;
	}

	Math::Vector3 ConversionBySelf(Math::Vector3& moveDirection)
	{
		if (!transform_)return Math::Vector3::Zero;
		Math::Vector3 moveDir = moveDirection;
		Math::Vector3 selfForward = transform_->GetForward();
		selfForward.y = 0;
		selfForward.Normalize();
		const float yaw = std::atan2(selfForward.x, selfForward.z);
		const Math::Quaternion yawOnly =
			Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yaw);
		moveDir = Math::Vector3::Transform(moveDir, yawOnly);

		return moveDir;
	}

	Math::Vector3 moveDir_{};
	TransformComponent* transform_ = nullptr;
	IRawDirectionSource* source_ = nullptr;
	ConversionMode mode_ = ConversionMode::camera;

	// activeCameraの水平前方ベクトルがこれ以下(ほぼ真上/真下を向いている)
	// の場合は、そこからyawを決めずCameraOrbitComponent側にフォールバックする、
	// という閾値。
	static constexpr float kMinCameraForwardLengthSq = 1e-6f;
};