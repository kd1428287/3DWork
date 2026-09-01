#pragma once
#include "../../BehaviorTree/IBTNode.h"

class EnemyAIController;

// ============================================================
// WarrockBehaviorが使う「判断層固有」のActionをまとめたファイル。
// 実行層(EnemyAIController)は全敵種で共通のため、ここではIBTNode<
// EnemyAIController>を実装する(以前のWarrockAIController専用
// テンプレートは廃止した。WarrockBehavior.h冒頭コメント参照)。
//
// 攻撃・被弾リアクション・咆哮の実行部分は共通の汎用BTノード
// (BTWeightedAttackAction<T>/BTOneShotAnimationAction<T>)へ切り出し
// 済みのため、ここに残っているのはIdle(待機)とChase(追跡)という、
// Warrock固有の「間合いの取り方」の判断そのものだけ。
// ============================================================

// 「待機」: Warrockは持ち場から動かないボスのため巡回は行わず、
// その場で待ち続けるだけの実装にする。Selectorの最後の子として
// 他が全てFailureの間、フォールバック先になり続ける。
class WarrockActionIdle : public IBTNode<EnemyAIController>
{
public:
	BTNodeStatus Tick(EnemyAIController* context, float deltaTime) override;
};

// 「追跡」: ターゲットの方向へ移動し続ける。HasTarget()がfalseになった
// 瞬間にFailureを返し、Selectorが次フレームで待機へ自然にフォールバック
// する。
class WarrockActionChase : public IBTNode<EnemyAIController>
{
public:
	BTNodeStatus Tick(EnemyAIController* context, float deltaTime) override;
};
