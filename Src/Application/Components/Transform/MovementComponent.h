#pragma once
#include <cstdio>

#include "IMovementSource.h"
#include "TransformComponent.h"

// ============================================================
// サンプルコンポーネント その2: Movement
//
// 「動く」という処理自体(Transformを書き換える部分)と、
// 「どちらに動くか決める」処理(IMovementSource)を分離している。
// これにより、手動操作(入力コンポーネント)・自動操作(AIコンポーネント)を
// 同じMovementComponentでそのまま扱える。
//
// 依存コンポーネントの取得は Start() で行うのが定石
// (Awake順序に依らず、全コンポーネントの追加が終わってから解決できるため)。
// ============================================================
class MovementComponent : public ComponentBase {
public:
	explicit MovementComponent(GameObject* owner, float speed = 1.0f)
		: ComponentBase(owner), speed_(speed) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		if (transform_ == nullptr) {
			std::printf(
				"[MovementComponent] warning: %s has no TransformComponent\n",
				GetOwner()->GetName().c_str());
		}
		if (source_ == nullptr) {
			std::printf(
				"[MovementComponent] warning: %s has no IMovementSource "
				"(SetMovementSource() を呼んでください)\n",
				GetOwner()->GetName().c_str());
		}
	}

	void Update(float deltaTime) override {
		if (transform_ == nullptr || source_ == nullptr) return;

		const Math::Vector3 v = source_->GetDesiredVelocity(deltaTime);
		transform_->position.x += v.x * speed_ * deltaTime;
		transform_->position.y += v.y * speed_ * deltaTime;
		transform_->position.z += v.z * speed_ * deltaTime;
	}

	// 動きの決定方法(手動入力 / AI など)を差し替える。
	// PlayerInputComponent* や AIWanderComponent* など、
	// IMovementSourceを実装したコンポーネントを渡す。
	void SetMovementSource(IMovementSource* source) { source_ = source; }

	void SetSpeed(float speed) { speed_ = speed; }
	float GetSpeed() const { return speed_; }

private:
	float speed_;
	TransformComponent* transform_ = nullptr;
	IMovementSource* source_ = nullptr;
};