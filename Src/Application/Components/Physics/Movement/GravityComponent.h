#pragma once

#include "../Movement/VelocityComponent.h"
#include "../Sensors/GroundSensorComponent.h" 

// 重力を適用するコンポーネント。
class GravityComponent : public ComponentBase {
public:
	// gravityAcceleration: Y軸方向の加速度(m/s^2相当)。
	// 下向きに落としたいので通常は負の値を渡す。
	explicit GravityComponent(GameObject* owner, float gravityAcceleration = -20.0f)
		: ComponentBase(owner), gravityAcceleration_(gravityAcceleration) {}

	void Awake() override {
		velocity_ = GetOwner()->GetComponent<VelocityComponent>();
		groundSensor_ = GetOwner()->GetComponent<GroundSensorComponent>();
	}

	void Update(float deltaTime) override {
		if (velocity_ == nullptr) return;

		if (groundSensor_ != nullptr) {
			if (groundSensor_->JustLanded()) {
				Math::Vector3 continuousVelocity = velocity_->GetContinuousVelocity();
				continuousVelocity.y = 0.0f;
				velocity_->SetContinuousVelocity(continuousVelocity);
			}

			// 接地中は加算しない
			if (groundSensor_->IsGrounded()) {
				return;
			}
		}

		velocity_->AddContinuousVelocity({ 0.0f, gravityAcceleration_ * deltaTime, 0.0f });
	}

	void SetGravityAcceleration(float gravityAcceleration) { gravityAcceleration_ = gravityAcceleration; }
	float GetGravityAcceleration() const { return gravityAcceleration_; }

private:
	VelocityComponent* velocity_ = nullptr;
	GroundSensorComponent* groundSensor_ = nullptr; // 無くてもよい(任意)
	float gravityAcceleration_;
};