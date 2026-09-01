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
// ├─ Sequence: Condition(被弾リアクション要求中?) → BTOneShotAnimationAction<T>(SmallReaction)
// ├─ Sequence: Condition(咆哮要求中?)             → BTOneShotAnimationAction<T>(Roaring)
// ├─ Sequence: Condition(攻撃間合い内?)           → BTWeightedAttackAction<EnemyAIController>
// ├─ Sequence: Condition(ターゲットを検知中?)     → WarrockActionChase
// └─ WarrockActionIdle (待機。Warrockは持ち場を離れないため巡回は無い)
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
// ============================================================
class WarrockBehavior : public IEnemyBehavior
{
public:
	std::unique_ptr<IBTNode<EnemyAIController>> BuildTree(EnemyAIController* owner) override;
	void OnSpawned(EnemyAIController* owner) override;
	void OnHit(EnemyAIController* owner, const AttackSourceComponent& attack) override;
	void OnDied(EnemyAIController* owner) override;
	float GetDespawnDelay() const override;

private:
	// 割り込み行動の要求フラグ(クラス冒頭コメント参照)。
	bool hitReactionPending_ = false;
	bool roarPending_ = false;
};
