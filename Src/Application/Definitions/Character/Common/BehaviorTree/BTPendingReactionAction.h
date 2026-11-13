#pragma once
#include "IBTNode.h"
#include "../CharacterDefinitionCommon.h" // MotionClipData

// 複数ある割り込み演出(被弾/大スタン/パリィされ/咆哮等)のうち、
// 「今要求されている1本」を再生してSuccessを返す汎用Action。
// 再生するクリップはTick()のたびにresolveClipで問い合わせるため、1ノードで複数の演出を扱える。
//
// 優先度の解決は呼び出し側の責務(WarrockBehavior::RequestReaction()等)。
// resolveClipは常に「今もっとも優先すべきクリップ」を返す前提。
// 前回と違うクリップが返ってきたら、優先度の高い要求に差し替わったとみなして最初から再生し直す。
//
// Tに継承は不要。以下を持っていればよい:
//   void StopMovement()
//   void PlayAnimation(const std::string&, bool, float, bool)
template <typename T>
class BTPendingReactionAction : public IBTNode<T>
{
public:
	// resolveClip: 再生すべきクリップ(無ければnullptr)。onClear: 再生完了時に要求状態をクリアする。
	using ResolveClipFn = std::function<const MotionClipData* (T*)>;
	using ClearFn = std::function<void(T*)>;

	BTPendingReactionAction(ResolveClipFn resolveClip, ClearFn onClear)
		: resolveClip_(std::move(resolveClip)), onClear_(std::move(onClear)) {
	}

	BTNodeStatus Tick(T* context, float deltaTime) override
	{
		const MotionClipData* clip = resolveClip_(context);
		if (clip == nullptr) return BTNodeStatus::Failure;

		if (clip != playingClip_) {
			playingClip_ = clip;
			elapsed_ = 0.0f;
			context->StopMovement();
			context->PlayAnimation(clip->animationName, false, clip->duration, clip->useRootMotion);
		}

		elapsed_ += deltaTime;
		if (elapsed_ >= clip->duration) {
			playingClip_ = nullptr;
			if (onClear_) onClear_(context);
			return BTNodeStatus::Success;
		}
		return BTNodeStatus::Running;
	}

	// 中断時は要求を消費せず、次回最初から再生し直す
	void Reset() override { playingClip_ = nullptr; elapsed_ = 0.0f; }

private:
	ResolveClipFn resolveClip_;
	ClearFn onClear_;

	const MotionClipData* playingClip_ = nullptr;
	float elapsed_ = 0.0f;
};
