#pragma once

// 外力によって駆動される「速度」を管理するコンポーネント。
class VelocityComponent : public ComponentBase {
public:
	// dampingPerSecond: impulseVelocity_が1秒間でこの割合まで落ちる減衰係数。
	explicit VelocityComponent(GameObject* owner, float dampingPerSecond = 0.05f)
		: ComponentBase(owner), dampingPerSecond_(dampingPerSecond) {
	};

	void DampingUpdate(float deltaTime)
	{
		// 摩擦減衰
		const float decay = std::pow(dampingPerSecond_, deltaTime);
		impulseVelocity_ *= decay;

		if (impulseVelocity_.LengthSquared() < kStopThresholdSq) {
			impulseVelocity_ = Math::Vector3::Zero;
		};
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

	Math::Vector3 impulseVelocity_ = Math::Vector3::Zero;
	Math::Vector3 continuousVelocity_ = Math::Vector3::Zero;
	float dampingPerSecond_;
};