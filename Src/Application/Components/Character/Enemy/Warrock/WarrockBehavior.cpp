#include "WarrockBehavior.h"
#include "../../BehaviorTree/BTWeightedAttackAction.h"
#include "../EnemyAIController.h"
#include "../EnemyActions.h"
#include "../../BehaviorTree/BTComposite.h"
#include "../../BehaviorTree/BTCondition.h"
#include "../../BehaviorTree/BTOneShotAnimationAction.h"
#include "../../../Collision/AttackSourceComponent.h"

namespace
{
	// 各種割り込み/死亡演出の再生尺。実アニメーションの尺に合わせて
	// 調整すること(【要確認】、値自体はまだ未検証)。
	constexpr float kHitReactionDuration = 0.4f;
	constexpr float kRoarDuration = 1.5f;
	constexpr float kDyingDuration = 2.0f;

	// 体幹崩壊時の大スタン再生尺。専用アニメーションが未実装のため、
	// 小スタンと同じ"miniStun"クリップを暫定的に使い回す
	// (PlayerStatusController::StateStagger::Enter()と同じ考え方)。
	// 尺だけ小スタン(kHitReactionDuration)より長くして「大きく怯んで
	// いる」ことを表現する。【要確認】専用アニメーションが用意でき
	// 次第差し替えること。
	constexpr float kBigStaggerDuration = 3.f;

	// Dyingアニメーション終了後、さらに少し間を置いてから消滅させる
	// (ボスの死亡演出として唐突に消えないようにするための余白)。
	constexpr float kPostDeathLingerSeconds = 1.0f;
}

std::unique_ptr<IBTNode<EnemyAIController>> WarrockBehavior::BuildTree(EnemyAIController* /*owner*/)
{
	// 被弾リアクション/咆哮は「要求フラグが立ったら1本再生して消費する」
	// という同じ形の割り込み行動のため、汎用BTOneShotAnimationAction<T>
	// (BTOneShotAnimationAction.h参照)をラムダで用途ごとに使い分ける形
	// にしている。フラグ自体はこのWarrockBehaviorインスタンスのメンバ
	// (クラス冒頭コメント参照)。
	// 「大スタン」: 体幹が尽きた(崩し発生、OnHit()参照)瞬間に、他の
	// どの行動中でも割り込んで発生させたいため、Selectorの最優先
	// (先頭)に置く。reactiveなSelectorが毎フレーム先頭から評価し直す
	// ため、被弾リアクション/咆哮/攻撃/追跡/待機のどれを実行中でも
	// 次フレームには必ずこちらへ切り替わる(攻撃Windup/Active中だった
	// 場合はSelectorがBTWeightedAttackAction<T>::Reset()を呼ぶため、
	// Active中に割り込んでもHitBoxは正しく閉じられる)。
	// 専用アニメーションは未実装のため、暫定的に小スタンと同じ
	// "miniStun"を尺だけ変えて再生する(namespace内コメント参照)。
	auto bigStaggerSeq = std::make_unique<BTSequence<EnemyAIController>>();
	bigStaggerSeq->AddChild(std::make_unique<BTCondition<EnemyAIController>>(
		[this](EnemyAIController*) { return bigStaggerPending_; }));
	bigStaggerSeq->AddChild(std::make_unique<BTOneShotAnimationAction<EnemyAIController>>(
		[this](EnemyAIController* c) {
			c->StopMovement();
			c->PlayAnimation("SmallReaction", false, kBigStaggerDuration);
			// 大スタンが優先されるため、同時に立っていたかもしれない
			// 小スタン(被弾リアクション)の要求は消費せず破棄する
			// (大スタンの後にさらに小スタンへ入り直す二重反応を防ぐ)。
			hitReactionPending_ = false;
		},
		kBigStaggerDuration,
		[this](EnemyAIController* c) {
			bigStaggerPending_ = false;
			// 崩し状態を演出し終えたので体幹をリセットし、次の削り合いに
			// 備える(PostureComponent::Reset()参照。ここで呼ばないと
			// IsBroken()==trueのまま残り、毎フレーム大スタンへ入り直そう
			// としてしまう)。
			if (PostureComponent* posture = c->GetPostureComponent()) {
				posture->Reset();
			}
		}));

	auto parriedSeq = std::make_unique<BTSequence<EnemyAIController>>();
	parriedSeq->AddChild(std::make_unique<BTCondition<EnemyAIController>>(
		[this](EnemyAIController*) {return parriedPending_; }));
	parriedSeq->AddChild(std::make_unique<BTOneShotAnimationAction<EnemyAIController>>(
		[this](EnemyAIController* c) {
			c->StopMovement();
			c->PlayAnimation("SmallReaction", false, kHitReactionDuration);
		},
		kHitReactionDuration,
		[this](EnemyAIController*) {parriedPending_ = false; }));

	auto reactionSeq = std::make_unique<BTSequence<EnemyAIController>>();
	reactionSeq->AddChild(std::make_unique<BTCondition<EnemyAIController>>(
		[this](EnemyAIController*) { return hitReactionPending_; }));
	reactionSeq->AddChild(std::make_unique<BTOneShotAnimationAction<EnemyAIController>>(
		[](EnemyAIController* c) {
			c->StopMovement();
			c->PlayAnimation("SmallReaction", false, kHitReactionDuration);
		},
		kHitReactionDuration,
		[this](EnemyAIController*) { hitReactionPending_ = false; }));

	auto roarSeq = std::make_unique<BTSequence<EnemyAIController>>();
	roarSeq->AddChild(std::make_unique<BTCondition<EnemyAIController>>(
		[this](EnemyAIController*) { return roarPending_; }));
	roarSeq->AddChild(std::make_unique<BTOneShotAnimationAction<EnemyAIController>>(
		[](EnemyAIController* c) {
			c->StopMovement();
			c->PlayAnimation("Roaring", false, kRoarDuration);
		},
		kRoarDuration,
		[this](EnemyAIController*) { roarPending_ = false; }));


	auto attackSeq = std::make_unique<BTSequence<EnemyAIController>>();
	attackSeq->AddChild(std::make_unique<BTCondition<EnemyAIController>>(
		[](EnemyAIController* c) { return c->IsTargetInAttackRange() && !c->IsAttackOnCooldown(); }));
	attackSeq->AddChild(std::make_unique<BTWeightedAttackAction<EnemyAIController>>());

	auto chaseSeq = std::make_unique<BTSequence<EnemyAIController>>();
	chaseSeq->AddChild(std::make_unique<BTCondition<EnemyAIController>>(
		[](EnemyAIController* c) { return c->HasTarget(); }));
	chaseSeq->AddChild(std::make_unique<EnemyActionMaintainDistance>());

	// 優先度順
	auto selector = std::make_unique<BTSelector<EnemyAIController>>();
	selector->AddChild(std::move(bigStaggerSeq));
	selector->AddChild(std::move(parriedSeq));
	selector->AddChild(std::move(reactionSeq));
	selector->AddChild(std::move(roarSeq));
	selector->AddChild(std::move(attackSeq));
	selector->AddChild(std::move(chaseSeq));
	selector->AddChild(std::make_unique<WarrockActionIdle>());

	return selector;
}

void WarrockBehavior::OnSpawned(EnemyAIController* /*owner*/)
{
	roarPending_ = true;
}

void WarrockBehavior::OnHit(EnemyAIController* owner, const AttackSourceComponent& attack)
{
	hitReactionPending_ = true;

	if (PostureComponent* posture = owner->GetPostureComponent()) {
		posture->AddPostureDamage(attack.postureDamage);

		// 体幹が尽きた(=崩し発生)。実際に大スタンとして割り込ませる
		if (posture->IsBroken()) {
			bigStaggerPending_ = true;
		}
	}
}

// --- 被パリィ処理 ---------------------------------------------------------
void WarrockBehavior::OnParried(EnemyAIController* owner, const AttackSourceComponent::ParriedEvent& event)
{
	parriedPending_ = true;

	if (PostureComponent* posture = owner->GetPostureComponent()) {
		posture->AddPostureDamage(event.parryPostureDamage);
		if (posture->IsBroken()) {
			bigStaggerPending_ = true;
		}
	}
}

void WarrockBehavior::OnDied(EnemyAIController* owner)
{
	owner->StopMovement();
	owner->PlayAnimation("Dying", false, kDyingDuration);
}

float WarrockBehavior::GetDespawnDelay() const
{
	return kDyingDuration + kPostDeathLingerSeconds;
}