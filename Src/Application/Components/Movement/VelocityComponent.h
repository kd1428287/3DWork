#pragma once
#include <cstdio>
#include <cmath>

#include "../Transform/TransformComponent.h"

// 外力によって駆動される「速度」を管理するコンポーネント。
class VelocityComponent : public ComponentBase {
public:
	// dampingPerSecond: impulseVelocity_が1秒間でこの割合まで落ちる減衰係数。
	explicit VelocityComponent(GameObject* owner, float dampingPerSecond = 0.05f)
		: ComponentBase(owner), dampingPerSecond_(dampingPerSecond) {
	}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		if (transform_ == nullptr) {
			std::printf(
				"[VelocityComponent] warning: %s has no TransformComponent\n",
				GetOwner()->GetName().c_str());
		}
	}

	void Update(float deltaTime) override {
		if (transform_ == nullptr) return;

		const float clampedDeltaTime = std::min(deltaTime, kMaxDeltaTime);

		if (continuousVelocity_.LengthSquared() > kMaxSpeed * kMaxSpeed) {
			continuousVelocity_.Normalize();
			continuousVelocity_ *= kMaxSpeed;
		}

		const Math::Vector3 totalVelocity = impulseVelocity_ + continuousVelocity_;
		if (totalVelocity.LengthSquared() > kStopThresholdSq) {
			transform_->Translate(totalVelocity * clampedDeltaTime);
		}

		// 摩擦減衰
		const float decay = std::pow(dampingPerSecond_, clampedDeltaTime);
		impulseVelocity_ *= decay;

		if (impulseVelocity_.LengthSquared() < kStopThresholdSq) {
			impulseVelocity_ = Math::Vector3::Zero;
		}
	}

	void AddImpulse(const Math::Vector3& impulse) { impulseVelocity_ += impulse; }
	void AddContinuousVelocity(const Math::Vector3& delta) { continuousVelocity_ += delta; }
	void SetContinuousVelocity(const Math::Vector3& velocity) { continuousVelocity_ = velocity; }
	const Math::Vector3& GetContinuousVelocity() const { return continuousVelocity_; }

	void ClearContinuousVelocity() { continuousVelocity_ = Math::Vector3::Zero; }
	void SetImpulseVelocity(const Math::Vector3& velocity) { impulseVelocity_ = velocity; }
	const Math::Vector3& GetImpulseVelocity() const { return impulseVelocity_; }

	Math::Vector3 GetVelocity() const { return impulseVelocity_ + continuousVelocity_; }

	bool IsMoving() const {
		return (impulseVelocity_ + continuousVelocity_).LengthSquared() > kStopThresholdSq;
	}

	bool IsImpulseActive() const {
		return impulseVelocity_.LengthSquared() > kStopThresholdSq;
	}

	void SetDampingPerSecond(float damping) { dampingPerSecond_ = damping; }
	float GetDampingPerSecond() const { return dampingPerSecond_; }

private:
	static constexpr float kStopThresholdSq = 0.0001f;
	static constexpr float kMaxDeltaTime = 1.0f / 30.0f;
	static constexpr float kMaxSpeed = 25.0f;

	TransformComponent* transform_ = nullptr;
	Math::Vector3 impulseVelocity_ = Math::Vector3::Zero;
	Math::Vector3 continuousVelocity_ = Math::Vector3::Zero;
	float dampingPerSecond_;
};