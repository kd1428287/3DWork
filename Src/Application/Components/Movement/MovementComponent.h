#pragma once
#include <cstdio>

#include "IMovementSource.h"
#include "../Transform/TransformComponent.h"

class MovementComponent : public ComponentBase {
public:
	explicit MovementComponent(GameObject* owner, float speed = 1.0f)
		: ComponentBase(owner), speed_(speed) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	void Update(float deltaTime) override {
		if (transform_ == nullptr || source_ == nullptr) return;

		const Math::Vector3 v = source_->GetDesiredVelocity();
		transform_->Translate(v * (speed_ * deltaTime));
	}

	// 動きの決定方法(手動入力 / AI など)を差し替える。
	void SetMovementSource(IMovementSource* source) { source_ = source; }

	void SetSpeed(float speed) { speed_ = speed; }
	float GetSpeed() const { return speed_; }

private:
	float speed_;
	TransformComponent* transform_ = nullptr;
	IMovementSource* source_ = nullptr;
};