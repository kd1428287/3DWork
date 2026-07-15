#pragma once

#include "../Transform/TransformComponent.h"
#include "VelocityComponent.h"

// ============================================================
// 開始位置→終了位置へ、指定時間で遷移させるコンポーネント。
// カードドロー、UI要素の移動、演出的な移動全般に向く。
// MovementComponentとは違い「一回限りで完了する」ことが前提。
//
// VelocityComponent(外力による移動)が同居していて、かつ外力が
// 働いている間は、Tweenの進行(elapsed_)自体を一時停止する。
// 位置だけ止めてelapsed_を進め続けると、外力が収まった瞬間に
// 理想軌道へワープする不自然な挙動になるため。
// ============================================================
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
		if (velocityComponent_ != nullptr && velocityComponent_->IsMoving()) return;

		elapsed_ += deltaTime;
		float t = std::min(elapsed_ / duration_, 1.0f);
		float eased = EaseOutCubic(t);  // カーブは差し替え可能

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