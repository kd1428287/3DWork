#pragma once
#include "Application/Definitions/Character/Enemy/IEnemyBehavior.h"

// ============================================================
// 【変更】以前はWarrockActionIdle/WarrockActionChase(WarrockActions.h)を
// includeしていたが、両クラスとも実質的にEnemyActions.hの汎用Action
// (EnemyActionIdle/EnemyActionMaintainDistance)と内容が重複/未使用の
// デッドコードだったため削除した(WarrockActions.h/.cppファイル自体も
// 廃止)。BuildTree()はEnemyActions.h側のクラスを直接使う。
//
// 【被弾リアクション/大スタン/パリィ/咆哮の統合について】
// 以前はhitReactionPending_/bigStaggerPending_/parriedPending_/
// roarPending_という独立したbool4本と、それぞれに対応する
// 「Condition+BTOneShotAnimationAction」のSequenceを4本持っていたが、
// 新しい演出を1つ増やすたびにこの3点セットをコピペする必要があり、
// しかもbigStaggerSeqの中で他のフラグ(hitReactionPending_)を直接
// 触って優先度を手動調整する箇所もあった(スケールしない設計)。
// 「今どの演出が要求されているか」をpending_という単一の状態に統合し、
// 優先度の解決(RequestReaction())と実際の再生(BTPendingReactionAction)
// を分離する形にした。
// ============================================================
class WarrockBehavior : public IEnemyBehavior
{
public:
	std::unique_ptr<IBTNode<EnemyAIController>> BuildTree(EnemyAIController* owner) override;
	void OnSpawned(EnemyAIController* owner) override;
	void OnStaggered(EnemyAIController* owner, bool isLarge, float duration) override;
	void OnParried(EnemyAIController* owner, const AttackSourceComponent::ParriedEvent& event) override;
	void OnDied(EnemyAIController* owner) override;
	float GetDespawnDelay(const EnemyAIController* owner) const override;

private:
	// 数値が小さいほど優先度が高い(0が最優先)。以前のSelectorの並び順
	// (bigStagger→parried→reaction→roar)が暗黙に表現していた優先順位を、
	// この数値として明示的に持つ形に置き換えた。
	enum class ReactionPriority : int {
		BigStagger = 0,
		Parried = 1,
		HitReaction = 2,
		Roar = 3,
	};

	// 新しい割り込み演出を要求する。既に何か要求中で、かつそちらの方が
	// 優先度が高い(数値が小さい)場合は上書きしない。
	//
	// 【ownerを受け取る理由】要求の有無(EnemyBlackboardKeys::
	// HasPendingReaction)をBlackboardへも書き込むため
	// (BuildTree()側のreactionSeqをBTCompareBoolCondition化した際、
	// pending_というWarrockBehavior内部の状態を直接見に行けなくなった
	// ことへの対応)。
	//
	// 【既知の簡略化】これは「今もっとも優先度の高い1件」だけを保持する
	// 単一スロット方式であり、優先度の低い要求(例: Roar)が再生中に、より
	// 優先度の高い要求(例: BigStagger)で上書き・中断された場合、Roarの
	// 要求はキューに残らず消滅する(旧実装のbool版は、中断されても
	// フラグ自体は消費されずに残り続けるため、割り込みが終わった後に
	// Roarが最初から再生し直されていた)。この違いが実際に問題になる
	// ケース(低頻度演出が高頻度演出に頻繁に割り込まれる敵種)が出てきたら、
	// 単一スロットではなく優先度付きキューへ拡張すること。
	void RequestReaction(EnemyAIController* owner, const std::string& oneShotAnimationKey, ReactionPriority priority);

	struct PendingReaction {
		std::string oneShotAnimationKey; // EnemyAIData::oneShotAnimationsのキー。空文字なら「要求無し」
		ReactionPriority priority = ReactionPriority::Roar;
	};
	PendingReaction pending_;
};