#pragma once
#include "IBTNode.h"
#include "../CharacterDefinitionCommon.h" // MotionClipData

// ============================================================
// 複数種類ある割り込み演出(被弾リアクション/大スタン/パリィされた時の
// リアクション/咆哮等)のうち「今どれが要求されているか」を呼び出し側
// (各敵種のIEnemyBehavior実装)が管理している前提で、該当するクリップを
// 1本再生してSuccessを返す汎用Action。
//
// 【この部品を作った経緯】
// WarrockBehavior::BuildTree()は以前、hitReactionSeq/bigStaggerSeq/
// parriedSeq/roarSeqという「Condition(pendingフラグを見る)+
// BTOneShotAnimationAction」の組を4本、ほぼ同じ形でコピペしていた。
// BTOneShotAnimationActionは「アニメーション名・尺」をBuildTree()の
// 時点でラムダの外から固定できる場合(咆哮なら常にRoaring、等)に使う
// 部品だが、「複数の演出候補のうちどれが有効か」はTick時点にならないと
// 決まらず、しかも尺も演出ごとに異なるため、コンストラクタで尺を
// 固定するBTOneShotAnimationActionでは1本にまとめられなかった。
// このノードは「今再生すべきクリップは何か」自体をラムダ経由でTick()の
// 中で都度問い合わせることで、1本のノードで複数種類の割り込み演出を
// まとめて扱えるようにする。
//
// 【優先度の解決について】
// 「今どの演出が一番優先されるべきか」の判断(例: 大スタン>パリィ>被弾
// リアクション>咆哮)はこのノードの責務にしない。resolveClip_が返す
// クリップが常に「今もっとも優先すべき演出」であることは、呼び出し側
// (WarrockBehavior::RequestReaction()等)が要求を受け取る時点で保証する。
//
// 【再生中に、より優先度の高い要求へ差し替わった場合の割り込みについて】
// 前回Tickで再生していたクリップ(playingClip_)と、今回resolveClip_が
// 返したクリップのポインタを比較し、異なっていれば「新しい要求(=より
// 優先度の高い要求)に差し替わった」とみなして現在の再生を打ち切り、
// 新しいクリップを最初から再生し直す。同一クリップを指し続けている間は
// 何もせず、経過時間だけを進める。
//
// Tには継承を要求しない(BTWeightedAttackAction等と同じダックタイピング
// 方針)。以下のメンバ関数を持っていればよい:
//   void StopMovement()
//   void PlayAnimation(const std::string&, bool, float, bool)
// ============================================================
template <typename T>
class BTPendingReactionAction : public IBTNode<T>
{
public:
	// resolveClip: 今再生すべきクリップが無ければnullptr。
	// onClear: 再生完了時に、呼び出し側の要求状態をクリアする。
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

	// 中断された場合、要求自体(呼び出し側が持つpending状態)は消費しない
	// (再生をやり切っていないため。BTOneShotAnimationAction::Reset()と
	// 同じ考え方)。次に選ばれた時にplayingClip_ == nullptrとなり、
	// 最初から再生し直される。
	void Reset() override { playingClip_ = nullptr; elapsed_ = 0.0f; }

private:
	ResolveClipFn resolveClip_;
	ClearFn onClear_;

	const MotionClipData* playingClip_ = nullptr;
	float elapsed_ = 0.0f;
};
