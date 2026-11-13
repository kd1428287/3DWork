#pragma once
#include "../Common/BehaviorTree/IBTNode.h"
#include "Application/Components/GamePlay/Character/Common/AttackSourceComponent.h"
#include "Application/Core/EventBus/Events/CombatEvents.h"

class EnemyAIController;

struct EnemyBehaviorConfig {

};

// 敵種ごとの判断(BTの構築)と固有の反応(被弾/死亡/登場時)をまとめたインターフェース。
// 移動・アニメ・索敵・武器制御などの実行層はEnemyAIControllerが共通で持つ。
// 現状はWarrockBehaviorの要件を基準に形を決めており、BruteBehaviorは追従できていない部分がある。
class IEnemyBehavior
{
public:
	virtual ~IEnemyBehavior() = default;

	// BTを組み立てる。EnemyAIController::Awake()から1回だけ呼ばれる。
	virtual std::unique_ptr<IBTNode<EnemyAIController>> BuildTree(EnemyAIController* owner) = 0;

	// 生成直後(BuildTree()の後)に1回だけ呼ばれる。例: Warrockの登場時Roar。
	virtual void OnSpawned(EnemyAIController* owner) {}

	// 通常被弾の処理後に、EnemyAIController::EnterStagger()経由で呼ばれる。
	// ダメージ・体幹処理はHitReactionComponentが済ませており、isLargeは体幹が壊れたかを表す。
	// durationは多くの実装で無視してよい(演出尺はEnemyAIData::oneShotAnimationsが唯一の実体)。
	virtual void OnStaggered(EnemyAIController* owner, bool isLarge, float duration) {}
	//virtual void OnRestore(EnemyAIController* owner);

	// 自分の攻撃がパリィされた時に呼ばれる。
	virtual void OnAttackDeflected(EnemyAIController* owner, const Events::AttackDeflected& event) {}

	// 共通の死亡処理(移動停止・当たり判定無効化等)の直後に呼ばれる。Dyingアニメ再生などをここで行う。
	virtual void OnDied(EnemyAIController* owner) {}

	virtual void OnFatalized(EnemyAIController* owner) {}

	// 死亡から消滅までの猶予秒数。Dying尺+余白をowner->GetData()から算出するためownerを受け取る。
	virtual float GetDespawnDelay(const EnemyAIController* owner) const { return 1.5f; }
};