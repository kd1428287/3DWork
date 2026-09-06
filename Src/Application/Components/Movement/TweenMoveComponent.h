#pragma once

#include "../Transform/TransformComponent.h"
#include "VelocityComponent.h"

// 開始位置→終了位置へ、指定時間で遷移させるコンポーネント。
class TweenMoveComponent : public ComponentBase {
public:
	TweenMoveComponent(GameObject* owner, Math::Vector3 from, Math::Vector3 to, float duration)
		: ComponentBase(owner), from_(from), to_(to), duration_(duration) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		if (transform_) transform_->SetPosition(from_);

		// 無くてもよい(任意)。存在する場合のみ外力優先の一時停止に使う。
		velocityComponent_ = GetOwner()->GetComponent<VelocityComponent>();
	}

	void Update(float deltaTime) override {
		if (transform_ == nullptr || finished_) return;

		// 外力(ノックバックなど)が働いている間はTweenそのものを一時停止する。
		//if (velocityComponent_ != nullptr && velocityComponent_->IsImpulseActive()) return;

		elapsed_ += deltaTime;
		float t = std::min(elapsed_ / duration_, 1.0f);
		float eased = EaseOutCubic(t);

		Math::Vector3 resultPos;
		resultPos = Math::Vector3::Lerp(from_, to_, eased);
		transform_->SetPosition(resultPos);

		if (t >= 1.0f) {
			finished_ = true;
			if (onComplete_) onComplete_(); 
		}
	}

	void SetOnComplete(std::function<void()> callback) {
		onComplete_ = std::move(callback);
	}

	bool IsFinished() const { return finished_; }

private:
	static float EaseOutCubic(float t) { return 1.0f - std::pow(1.0f - t, 3.0f); }

	Math::Vector3 from_;
	Math::Vector3 to_;
	float duration_;
	float elapsed_ = 0.0f;
	bool finished_ = false;
	TransformComponent* transform_ = nullptr;
	VelocityComponent* velocityComponent_ = nullptr; // 無くてもよい(任意)
	std::function<void()> onComplete_;
};