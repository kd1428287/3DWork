#pragma once

#include "TransformComponent.h"

// ============================================================
// サンプルコンポーネント その2: Movement
// 他コンポーネント(TransformComponent)に依存する例。
// 依存コンポーネントの取得は Start() で行うのが定石
// (Awake順序に依らず、全コンポーネントの追加が終わってから解決できるため)。
// ============================================================
class MovementComponent : public ComponentBase {
public:
	MovementComponent(GameObject* owner, Math::Vector3 velocity)
		: ComponentBase(owner), velocity_(velocity) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		if (transform_ == nullptr) {
			std::printf(
				"[MovementComponent] warning: %s has no TransformComponent\n",
				GetOwner()->GetName().c_str());
		}
	}

	void Update(float deltaTime) override {
		if (transform_ == nullptr) return;
		transform_->position.x += velocity_.x * deltaTime;
		transform_->position.y += velocity_.y * deltaTime;
		transform_->position.z += velocity_.z * deltaTime;
	}

private:
	Math::Vector3 velocity_;
	TransformComponent* transform_ = nullptr;
};