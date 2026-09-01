#pragma once
#include <functional>
#include "IBTNode.h"

// ============================================================
// 「1本のアニメーションを最後まで再生してSuccessを返す」だけの
// 汎用の割り込み型Action。
//
// 【この部品を作った経緯】
// WarrockActionHitReaction(被弾リアクション)とWarrockActionRoar(咆哮)は
// 「要求フラグが立ったら1回再生し、再生完了時にフラグを消費する」という
// 全く同じ形をしていた(再生中かどうかのbool、経過時間、固定尺との比較、
// という3つのメンバも完全に一致)。加えて被弾リアクションのような割り込み
// 行動はWarrock以外の敵種でも今後実装予定のため、Warrock専用ファイルに
// 閉じ込めず、BehaviorTree/配下の汎用ノードとしてここに集約している。
//
// Tには継承もダックタイピングも要求しない。「何をどう再生するか
// (onStart)」「何秒で完了とみなすか(duration)」「完了時に何をするか
// (onComplete)」の3つを呼び出し側がラムダで注入する形にすることで、
// このノード自体はどんなT(EnemyAIController/WarrockAIController/将来の
// 敵種)にも依存しない。
// ============================================================
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

	// 中断された場合、次回選ばれた時にonStart_をやり直させるため
	// playing_をfalseへ戻す(再生中断時に途中のフレームから
	// 「未再生」扱いへ戻すのは他のActionのReset()と同じ考え方)。
	void Reset() override { playing_ = false; elapsed_ = 0.0f; }

private:
	StartFn onStart_;
	float duration_;
	CompleteFn onComplete_;

	bool playing_ = false;
	float elapsed_ = 0.0f;
};
