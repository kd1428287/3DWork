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
	constexpr float kBigStaggerDuration = 1.2f;

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

	// 攻撃の3フェーズ実行(Windup/Active/Recovery)はBruteBehaviorと
	// 完全に共通のBTWeightedAttackAction<T>を使う(BTWeightedAttackAction.h
	// 参照)。「どの技を使うか」はWarrockAIData::attacksの重みで、
	// 「間合いに入ったら攻撃を試みる」という判断自体はここでWarrock
	// 固有に定義している。
	//
	// 【攻撃インターバル】IsAttackOnCooldown()をIsTargetInAttackRange()と
	// 併せてConditionでチェックする。直前の攻撃のRecovery完了時に
	// EnemyAIController::NotifyAttackCompleted()(BTWeightedAttackAction<T>
	// から呼ばれる)がEnemyAIData::attackIntervalDuration分のクールダウンを
	// 開始しており、それが解消するまではこのSequence自体がFailureを返す。
	// Selectorはreactiveなので、その間は自然に下位のchaseSeq/Idleへ
	// フォールバックし、移動自体は制限されない。
	auto attackSeq = std::make_unique<BTSequence<EnemyAIController>>();
	attackSeq->AddChild(std::make_unique<BTCondition<EnemyAIController>>(
		[](EnemyAIController* c) { return c->IsTargetInAttackRange() && !c->IsAttackOnCooldown(); }));
	attackSeq->AddChild(std::make_unique<BTWeightedAttackAction<EnemyAIController>>());

	// 「追跡」はWarrock固有のWarrockActionChase(距離0まで詰め続ける実装)
	// ではなく、汎用のEnemyActionMaintainDistance(EnemyActions.h参照)を使う。
	// EnemyAIData::maintainDistance以下まで近づいたら接近をやめて停止する
	// ため、プレイヤーに密着し続けず一定の間合いを保つ。攻撃間合い内
	// (IsTargetInAttackRange())ではreactive SelectorがこのchaseSeqより
	// 先にattackSeqを評価するため、実際に技を出せる距離まで来れば通常通り
	// 攻撃が優先される。距離を詰めても攻撃インターバル中(IsAttackOnCooldown())
	// で攻撃を出せない間は、maintainDistanceの位置で足を止めて待つ形になる。
	auto chaseSeq = std::make_unique<BTSequence<EnemyAIController>>();
	chaseSeq->AddChild(std::make_unique<BTCondition<EnemyAIController>>(
		[](EnemyAIController* c) { return c->HasTarget(); }));
	chaseSeq->AddChild(std::make_unique<EnemyActionMaintainDistance>());

	// 優先度順(大スタン > 被弾リアクション > 咆哮 > 攻撃 > 追跡 > 待機)。
	// reactive版BTSelectorのため、毎フレーム必ず先頭(大スタン)
	// から評価し直す。
	auto selector = std::make_unique<BTSelector<EnemyAIController>>();
	selector->AddChild(std::move(bigStaggerSeq));
	selector->AddChild(std::move(reactionSeq));
	selector->AddChild(std::move(roarSeq));
	selector->AddChild(std::move(attackSeq));
	selector->AddChild(std::move(chaseSeq));
	selector->AddChild(std::make_unique<WarrockActionIdle>());

	return selector;
}

void WarrockBehavior::OnSpawned(EnemyAIController* /*owner*/)
{
	// 登場時の咆哮を1回要求する(第二フェーズ移行時の再利用も想定。
	// クラス冒頭コメント参照)。
	roarPending_ = true;
}

void WarrockBehavior::OnHit(EnemyAIController* owner, const AttackSourceComponent& attack)
{
	hitReactionPending_ = true;

	// 体幹を削る。AttackSourceComponent::postureDamageのコメント上は
	// 「ガードされた時」用の値だが、Warrockには現状ガード状態が無く
	// (常に素で殴られる)、直撃時の体幹削りに使う専用フィールドが他に
	// 無いため、ひとまずこちらを直撃時にも流用する
	// (【要確認】ガード専用の値を直撃と共用してよいかは設計次第。
	//  分けたくなったらAttackSourceComponentへ専用フィールドを追加すること)。
	if (PostureComponent* posture = owner->GetPostureComponent()) {
		posture->AddPostureDamage(attack.postureDamage);

		// 体幹が尽きた(=崩し発生)。実際に大スタンとして割り込ませる
		// 処理はBuildTree()側で実装済み(bigStaggerPending_参照)。
		if (posture->IsBroken()) {
			bigStaggerPending_ = true;
		}
	}
}

// --- 被パリィ処理 ---------------------------------------------------------
// 自分自身の攻撃がパリィされた時にEnemyAIController::OnParried()経由で
// 呼ばれる。攻撃自身のparryPostureDamage(パリィされた側の判定コードが
// AttackSourceComponentから読み取ってイベントに詰めてくれる値。
// AttackSourceComponent::ParriedEvent冒頭コメント参照)を、攻撃者=
// 自分自身の体幹へ適用する。IsBroken()ならOnHit()と同じく
// bigStaggerPending_を立て、パリィをきっかけに体幹が尽きたケースも
// そのまま大スタンの優先度に合流させる。
//
// parriedPending_自体(被パリィ専用の割り込み・アニメーション再生)を
// BuildTree()側で消費する処理はまだ未実装(次のステップ)。
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
	// Bossの死亡演出。Dyingアニメーションを再生し、GetDespawnDelay()で
	// 消滅までの猶予をこの尺に合わせて延長している(EnemyAIController::
	// OnDied()参照)。
	owner->StopMovement();
	owner->PlayAnimation("Dying", false, kDyingDuration);
}

float WarrockBehavior::GetDespawnDelay() const
{
	return kDyingDuration + kPostDeathLingerSeconds;
}