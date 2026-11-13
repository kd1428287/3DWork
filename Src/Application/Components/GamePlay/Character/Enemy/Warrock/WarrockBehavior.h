#pragma once
#include <functional>
#include <string>
#include "Application/Definitions/Character/Enemy/IEnemyBehavior.h"

struct EnemyAttackDefinition;

// Warrock(武器を持たない大型モンスター)専用の判断層。
// 被弾/大スタン/パリィされ/咆哮の割り込み演出は、pending_(今要求されている1件)で一元管理し、
// BTPendingReactionAction 1本で再生する。優先度の解決はRequestReaction()が行う。
class WarrockBehavior : public IEnemyBehavior
{
public:
	std::unique_ptr<IBTNode<EnemyAIController>> BuildTree(EnemyAIController* owner) override;
	void OnSpawned(EnemyAIController* owner) override;
	void OnStaggered(EnemyAIController* owner, bool isLarge, float duration) override;
	void OnAttackDeflected(EnemyAIController* owner, const Events::AttackDeflected& event) override;
	void OnDied(EnemyAIController* owner) override;
	void OnFatalized(EnemyAIController* owner) override;

	float GetDespawnDelay(const EnemyAIController* owner) const override;

private:
	// 数値が小さいほど優先度が高い(0が最優先)
	enum class ReactionPriority : int {
		BigStagger = 0,
		Deflected = 1,
		HitReaction = 2,
		Roar = 3,
	};

	struct PendingReaction {
		std::string oneShotAnimationKey;	// EnemyAIData::oneShotAnimationsのキー。空なら要求なし
		ReactionPriority priority = ReactionPriority::Roar;
	};

	// --- BT組み立て ---
	std::unique_ptr<IBTNode<EnemyAIController>> BuildReactionSeq();
	// 「rangeKeyが真 かつ クールダウン中でない」なら、chooserが選んだ技を出すSequence
	std::unique_ptr<IBTNode<EnemyAIController>> BuildAttackSeq(
		const std::string& rangeKey, std::function<const EnemyAttackDefinition*(EnemyAIController*)> chooser);

	// --- 割り込み演出 ---
	// 要求を登録する。既に要求中で、そちらの優先度が高ければ上書きしない。
	// ownerはBlackboard(HasPendingReaction)へも書き込むために受け取る。
	// 【既知の簡略化】保持するのは最優先の1件のみ。上書きされた低優先の要求は消える
	// (問題になる敵種が出たら優先度付きキューへ拡張する)。
	void RequestReaction(EnemyAIController* owner, const std::string& oneShotAnimationKey, ReactionPriority priority);
	// pending_のキーに対応するクリップを返す。要求なし・未定義キーならnullptr
	const MotionClipData* ResolvePendingClip(EnemyAIController* owner);
	// pending_を空にしてHasPendingReactionを下ろす
	void ClearPending(EnemyAIController* owner);

	PendingReaction pending_;
};
