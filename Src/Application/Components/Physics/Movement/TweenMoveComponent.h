#pragma once

// ============================================================
// TweenMoveComponent(演出移動)
//
// 開始位置→終了位置へ、指定時間で遷移させるコンポーネント。
//
// 内力・外力が「速度」を提供するのに対し、こちらは「絶対位置」を
// 提供する点が根本的に違う。そのため両者は足し合わせることが
// できず、Tweenが動作中は内力・外力に優越して位置を決める
// (この優越関係の裁定はMotionComposerComponentが行う)。
//
// 自分ではTransformを一切書き換えず、Advance()で進行だけを進めて
// 現在位置を公開する。以前はUpdate()内で直接SetPosition()していたが、
// それだとMotionComposerのPostUpdateに毎フレーム上書きされて
// Tweenが無効化されてしまう。
// ============================================================
class TweenMoveComponent : public ComponentBase {
public:
	TweenMoveComponent(GameObject* owner, Math::Vector3 from, Math::Vector3 to, float duration)
		: ComponentBase(owner), from_(from), to_(to), duration_(duration) {
		currentPosition_ = from_;
	}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		if (transform_) transform_->SetPosition(from_);
	}

	// 進行を進め、現在位置を更新する。
	// MotionComposerComponentが毎フレーム呼ぶ(コンポーネントの
	// 呼び出し順に依存しないよう、合成役が明示的に駆動する)。
	void Advance(float deltaTime) {
		if (finished_) return;

		elapsed_ += deltaTime;
		const float t = (duration_ > 0.0f) ? std::min(elapsed_ / duration_, 1.0f) : 1.0f;
		const float eased = EaseOutCubic(t);

		currentPosition_ = Math::Vector3::Lerp(from_, to_, eased);

		if (t >= 1.0f) {
			finished_ = true;
			if (onComplete_) onComplete_();
		}
	}

	void SetOnComplete(std::function<void()> callback) {
		onComplete_ = std::move(callback);
	}

	// 動作中(=内力・外力に優越して位置を決めてよい状態)かどうか。
	bool IsActive() const { return !finished_; }
	bool IsFinished() const { return finished_; }

	// Advance()で算出された、このフレームの絶対位置。
	const Math::Vector3& GetCurrentPosition() const { return currentPosition_; }

private:
	static float EaseOutCubic(float t) { return 1.0f - std::pow(1.0f - t, 3.0f); }

	Math::Vector3 from_;
	Math::Vector3 to_;
	float duration_;
	float elapsed_ = 0.0f;
	bool finished_ = false;
	Math::Vector3 currentPosition_ = Math::Vector3::Zero;
	TransformComponent* transform_ = nullptr;
	std::function<void()> onComplete_;
};