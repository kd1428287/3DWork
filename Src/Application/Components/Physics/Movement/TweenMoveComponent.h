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
//
// 【所有方式】以前はRequestAddComponent/RequestRemoveComponentで
// 使う瞬間だけアタッチ/デタッチしていたが、Owner側の生成時に一度だけ
// アタッチし、以降はenabled_フラグで動作可否を切り替える方式に変更。
// GetComponent<TweenMoveComponent>()が常に非nullを返すようになるため、
// 「今アクティブなTweenがあるか」の判定は必ずIsActive()で行うこと
// (存在チェックでは代用できない)。
// 新しいTweenを開始したい側はPlay()を呼ぶ(from/to/durationを積み直し、
// enabled_をtrueにする)。中断したい側はCancel()/SetEnabled(false)を呼ぶ。
// ============================================================
class TweenMoveComponent : public ComponentBase {
public:
	explicit TweenMoveComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	// 新しいTweenを開始する。呼び出した瞬間にTransformをfromへスナップし、
	// enabled_をtrueにする。onCompleteを積みたい場合はPlay()の後に
	// SetOnComplete()を呼ぶこと(Play()側でonComplete_はクリアされる)。
	void Play(const Math::Vector3& from, const Math::Vector3& to, float duration) {
		from_ = from;
		to_ = to;
		duration_ = duration;
		elapsed_ = 0.0f;
		currentPosition_ = from_;
		onComplete_ = nullptr;
		enabled_ = true;

		if (transform_) transform_->SetPosition(from_);
	}

	void Advance(float deltaTime) {
		if (!enabled_) return;

		elapsed_ += deltaTime;
		const float t = (duration_ > 0.0f) ? std::min(elapsed_ / duration_, 1.0f) : 1.0f;
		const float eased = EaseOutCubic(t);

		currentPosition_ = Math::Vector3::Lerp(from_, to_, eased);

		if (t >= 1.0f) {
			enabled_ = false;
			auto onComplete = std::move(onComplete_);
			onComplete_ = nullptr;
			if (onComplete) onComplete();
		}
	}

	void SetOnComplete(std::function<void()> callback) {
		onComplete_ = std::move(callback);
	}

	// 外部からの明示的な中断。onComplete_は呼ばれない
	// (以前のRequestRemoveComponent<TweenMoveComponent>()相当)。
	void Cancel() { enabled_ = false; }

	void SetEnabled(bool enabled) { enabled_ = enabled; }

	// 動作中(=内力・外力に優越して位置を決めてよい状態)かどうか。
	// コンポーネントの存在自体は常にtrueなので、稼働判定は必ずこちらを使う。
	bool IsActive() const { return enabled_; }

	// Advance()で算出された、このフレームの絶対位置。
	const Math::Vector3& GetCurrentPosition() const { return currentPosition_; }

private:
	static float EaseOutCubic(float t) { return 1.0f - std::pow(1.0f - t, 3.0f); }

	Math::Vector3 from_ = Math::Vector3::Zero;
	Math::Vector3 to_ = Math::Vector3::Zero;
	float duration_ = 0.0f;
	float elapsed_ = 0.0f;
	bool enabled_ = false;
	Math::Vector3 currentPosition_ = Math::Vector3::Zero;
	TransformComponent* transform_ = nullptr;
	std::function<void()> onComplete_;
};