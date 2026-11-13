#pragma once
#include "IBTNode.h"

// 1本のアニメを最後まで再生してSuccessを返す汎用の割り込みAction。
// 開始処理(onStart)・尺(duration)・完了処理(onComplete)を呼び出し側がラムダで注入するため、
// どのController型にも依存しない。
template <typename T>
class BTOneShotAnimationAction : public IBTNode<T>
{
public:
	using StartFn = std::function<void(T*)>;
	using CompleteFn = std::function<void(T*)>;

	BTOneShotAnimationAction(StartFn onStart, float duration, CompleteFn onComplete)
		: onStart_(std::move(onStart)), duration_(duration), onComplete_(std::move(onComplete)) {
	}

	BTNodeStatus Tick(T* context, float deltaTime) override
	{
		if (!playing_) {
			playing_ = true;
			elapsed_ = 0.0f;
			if (onStart_) onStart_(context);
		}

		elapsed_ += deltaTime;
		if (elapsed_ >= duration_) {
			playing_ = false;
			if (onComplete_) onComplete_(context);
			return BTNodeStatus::Success;
		}
		return BTNodeStatus::Running;
	}

	// 中断時は未再生に戻し、次回onStartからやり直させる
	void Reset() override { playing_ = false; elapsed_ = 0.0f; }

private:
	StartFn onStart_;
	float duration_;
	CompleteFn onComplete_;

	bool playing_ = false;
	float elapsed_ = 0.0f;
};
