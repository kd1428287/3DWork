#pragma once
#include "../IEnemyBehavior.h"
#include "WarrockActions.h"

// ============================================================
// Warrock(ボス)の判断層+固有反応。
// 実行層(移動・アニメーション・索敵・武器制御)はEnemyAIController
// (全敵種共通のコンポーネント)側にあり、このクラスは「どう行動を
// 選ぶか」(BuildTree())と「被弾/死亡/登場時に何をするか」だけを持つ。
// 現在デバッグ表示で動かしながらここの詳細を詰めている。
//
// 【判断層の構成】(BuildTree()参照)
// Selector(reactive、優先度順)
// ├─ Sequence: Condition(体幹崩壊=大スタン要求中?)  → BTOneShotAnimationAction<T>(miniStun流用、大スタン尺)
// ├─ Sequence: Condition(被パリィ要求中?)          → BTOneShotAnimationAction<T>(miniStun流用、被パリィ尺)
// ├─ Sequence: Condition(被弾リアクション要求中?) → BTOneShotAnimationAction<T>(SmallReaction)
// ├─ Sequence: Condition(咆哮要求中?)             → BTOneShotAnimationAction<T>(Roaring)
// ├─ Sequence: Condition(攻撃間合い内? かつ インターバル明け?) → BTWeightedAttackAction<EnemyAIController>
// ├─ Sequence: Condition(ターゲットを検知中?)     → EnemyActionMaintainDistance(汎用、EnemyActions.h)
// └─ WarrockActionIdle (待機。Warrockは持ち場を離れないため巡回は無い)
//
// 大スタン/被パリィをSelectorの最優先2枠に置くことで、被弾リアクション/
// 咆哮/攻撃/追跡/待機のどれを実行中でも、reactive Selectorが次フレーム
// で必ずこちらへ切り替える(攻撃Windup/Active中の割り込みはBTWeightedAttackAction<T>
// ::Reset()がHitBoxを正しく閉じるため安全)。被パリィをきっかけに体幹まで
// 尽きた場合は、より派手な大スタンの方を優先して再生する(bigStaggerSeqの
// onStart側でparriedPending_も破棄するため、大スタン後にさらに被パリィへ
// 入り直す二重反応は起きない)。
//
// 【追跡→間合い維持への変更について】
// 以前はWarrockActionChase(距離0まで詰め続けるWarrock専用実装)を使って
// いたが、汎用のEnemyActionMaintainDistanceに差し替えた。EnemyAIData::
// maintainDistance以下まで近づいたら接近をやめてその場に停止するため、
// 攻撃インターバル中(IsAttackOnCooldown())等、攻撃を出せない間に
// プレイヤーへ密着し続けず一定の間合いを保てる。実際に技を出せる距離
// まで来れば、reactive Selectorがこちらより優先度の高いattackSeqを
// 先に評価するため、通常通り攻撃が優先される。
// なおWarrockActionChase自体はWarrockActions.h/.cppに残しているが、
// このBuildTree()からは現在参照していない(使わなくなったので削除する
// なら合わせて対応すること)。
//
// 【割り込み行動(被弾リアクション/咆哮)の要求フラグについて】
// hitReactionPending_/roarPending_はこのWarrockBehaviorインスタンス
// 自身が持つ(1体のWarrockにつき1つのWarrockBehaviorインスタンスが
// 対応する前提。EnemyFactory::CreateAIController()参照)。OnHit()/
// OnSpawned()で立てたフラグを、BuildTree()内のCondition/
// BTOneShotAnimationActionのラムダがthisキャプチャ経由で直接読み書き
// する(EnemyAIController側は一切これらのフラグを知らない)。
//
// 【死亡演出について】
// Warrockはボスに相当するため、死亡処理に固有の演出を持たせたい
// という要望を踏まえ、OnDied()でDyingアニメーションを再生し、
// GetDespawnDelay()で消滅までの猶予をその演出の尺に合わせて延長する
// (EnemyAIController::OnDied()が実際の消滅タイマーに使う)。
//
// 【体幹・被パリィについて(現状: 体幹の組み込み+大スタン割り込み+
// パリィ購読+被パリィ割り込みまで実装済み)】
// OnHit()でPostureComponent::AddPostureDamage()を呼び体幹を削り
// (postureDamageフィールドを直撃時にも流用。WarrockBehavior.cpp参照)、
// IsBroken()(体幹が尽きた=崩し発生)を検知するとbigStaggerPending_を
// 立てる。BuildTree()の最優先分岐がこれを消費し、他のどの行動中でも
// 割り込んで大スタン(専用アニメーション未実装のため暫定的に"miniStun"
// を尺だけ変えて流用)を再生する。完了時にPostureComponent::Reset()を
// 呼び、次の削り合いに備える。
// 被パリィはOnParried()(IEnemyBehavior::OnParried()、EnemyAIController
// がAttackSourceComponent::ParriedEventを自身のローカルEventBusで購読して
// 転送してくる)で受け取り、parriedPending_を立てつつevent.
// parryPostureDamageを自分自身の体幹へ適用する(IsBroken()ならOnHit()と
// 同じくbigStaggerPending_も立てる)。BuildTree()側でparriedPending_を
// 大スタンに次ぐ優先度で消費し、他のどの行動中でも割り込んで被パリィ
// 反応(こちらも専用アニメーション未実装のため暫定的に"miniStun"を尺
// だけ変えて流用)を再生する。
// ============================================================
class WarrockBehavior : public IEnemyBehavior
{
public:
	std::unique_ptr<IBTNode<EnemyAIController>> BuildTree(EnemyAIController* owner) override;
	void OnSpawned(EnemyAIController* owner) override;
	void OnHit(EnemyAIController* owner, const AttackSourceComponent& attack) override;
	void OnParried(EnemyAIController* owner, const AttackSourceComponent::ParriedEvent& event) override;
	void OnDied(EnemyAIController* owner) override;
	float GetDespawnDelay() const override;

private:
	// 割り込み行動の要求フラグ(クラス冒頭コメント参照)。
	bool hitReactionPending_ = false;
	bool roarPending_ = false;

	// 体幹が尽きた(IsBroken())時に立てる。まだBuildTree()側で消費して
	// いない(次のステップで大スタン専用アクションとして実装する)。
	bool bigStaggerPending_ = false;

	// 被パリィ時に立てる。BuildTree()側で大スタンに次ぐ優先度で消費し、
	// 専用の割り込み反応(暫定的にminiStun流用)を再生する。
	bool parriedPending_ = false;
};