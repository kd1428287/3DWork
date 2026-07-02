#pragma once

#include "TransformComponent.h"

// ============================================================
// 開始位置→終了位置へ、指定時間で遷移させるコンポーネント。
// カードドロー、UI要素の移動、演出的な移動全般に向く。
// MovementComponentとは違い「一回限りで完了する」ことが前提。
// ============================================================
class TweenMoveComponent : public ComponentBase {
public:
	TweenMoveComponent(GameObject* owner, Math::Vector3 from, Math::Vector3 to, float duration)
		: ComponentBase(owner), from_(from), to_(to), duration_(duration) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		if (transform_) transform_->position = from_;
	}

	void Update(float deltaTime) override {
		if (transform_ == nullptr || finished_) return;

		elapsed_ += deltaTime;
		float t = std::min(elapsed_ / duration_, 1.0f);
		float eased = EaseOutCubic(t);  // カーブは差し替え可能

		transform_->position.x = Lerp(from_.x, to_.x, eased);
		transform_->position.y = Lerp(from_.y, to_.y, eased);
		transform_->position.z = Lerp(from_.z, to_.z, eased);

		if (t >= 1.0f) {
			finished_ = true;
			if (onComplete_) onComplete_();  // 「手札に加える」等をここで呼ぶ
		}
	}

	void SetOnComplete(std::function<void()> callback) {
		onComplete_ = std::move(callback);
	}

	bool IsFinished() const { return finished_; }

private:
	static float Lerp(float a, float b, float t) { return a + (b - a) * t; }
	static float EaseOutCubic(float t) { return 1.0f - std::pow(1.0f - t, 3.0f); }

	Math::Vector3 from_;
	Math::Vector3 to_;
	float duration_;
	float elapsed_ = 0.0f;
	bool finished_ = false;
	TransformComponent* transform_ = nullptr;
	std::function<void()> onComplete_;
};