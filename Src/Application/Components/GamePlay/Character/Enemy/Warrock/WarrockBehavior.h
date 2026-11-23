#pragma once
#include <functional>
#include <string>
#include "Application/Definitions/Character/Enemy/IEnemyBehavior.h"

struct EnemyAttackDefinition;

// ============================================================================
// 【未提供ファイルへの変更依頼】(内容の全文が無いため、差分だけをここに記す)
// 「HP0/体幹MAX → 崩れ → 致命演出 → Life消費」フローの実装に必要。反映しないとビルドが通らない。
//
//  対象ファイル                              | 内容
//  ------------------------------------------|----------------------------------------------------------
//  Tags/IHitReactionQuery.h                  | 追加(必須・既定実装付きなので他の実装クラスは変更不要):
//                                            |   virtual bool HandlesPostureBreak() const { return false; }
//  Enemy/IEnemyBehavior.h                    | 追加(必須・既定実装付き):
//                                            |   virtual void OnBroken(EnemyAIController* owner, bool byHealth) {}
//  Enemy/EnemyAIData.h                       | メンバ追加(必須):
//                                            |   float brokenDurationSeconds  = 5.0f;   // 致命されない時の崩れ継続時間
//                                            |   float brokenTimeoutHealRatio = 0.3f;   // HP0でタイムアウトした時に戻すHP割合
//                                            |   float fatalityTimeoutSeconds = 10.0f;  // FatalityFinishedが来ない時の保険
//  Enemy/EnemyBlackboardKeys.h               | キー追加(必須): IsBroken(既存キーと同じ宣言形式で。bool)
//  Common/WeaponSetComponent.h/.cpp          | (任意・推奨) 全スロットのHitBox/Trail停止APIを追加し、
//                                            |   EnemyAIController::AbortCurrentAction()のTODO箇所で呼ぶ。
//                                            |   (崩れで攻撃を中断した時に判定が残らないようにするため)
//  ComponentRegistrations.cpp                | BuildEnemyAIController等の敵生成箇所で、敵のLifeComponentへ
//   (BuildEnemyAIController)                 |   SetDeferToFatality(true) を呼ぶ(呼ばないと従来どおりDownで即Life消費)。
//  敵のPrefab/JSON(EnemyAIData.oneShotAnims) | oneShotAnimationsへ "Broken"(崩れ。ループ再生) と
//                                            |   "Fatalized"(被致命。1回再生・duration設定)を登録する。
//  プレイヤー側(致命入力を扱うクラス)         | 致命入力時: target->CanBeFatalized() を確認し true なら
//                                            |   target->NotifyLethalAnimation() を呼んで自分の致命アニメを再生。
//                                            |   終了時に敵のLocalEventBusへ Events::FatalityFinished を発行する。
//                                            |   (位置合わせが要るなら既存の Events::Syncronization を利用可)
//  ISyncroAnimation / IHitReactionQuery      | ISyncroAnimationに仮想デストラクタを追加済み。他に実装している
//   を実装している他クラス                    |   クラスがあれば再コンパイルのみ(挙動は不変)。
// ============================================================================

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
	// 崩れ(体幹MAX/HP0)に入った時。保留中の割り込み演出を捨て、崩れアニメ(ループ)を再生する。
	void OnBroken(EnemyAIController* owner, bool byHealth) override;

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
