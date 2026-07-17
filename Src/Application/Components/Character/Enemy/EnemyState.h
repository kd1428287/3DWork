#pragma once
#include "../StateMachine/StateMachine.h"

// 前方宣言
class EnemyStatusController;

// ============================================================
// IEnemyState
// PlayerStateと同じ共通StateMachine基盤(IState<EnemyStatusController>)を
// そのまま使う軽量Stateパターンのインターフェース。GameObjectにはアタッチ
// せず、EnemyStatusController内部でインスタンスとして保持される。
// 現状はEnemy固有の追加メンバは不要(AI判断ロジックが増えたらここに追加)。
// ============================================================
class IEnemyState : public IState<EnemyStatusController> {};

// 基準点から右方向(+X)へ歩く。patrolDistanceまで進んだらStateWalkLeftへ切り替える。
class StateWalkRight : public IEnemyState {
public:
	void Enter(EnemyStatusController* controller) override;
	void Update(EnemyStatusController* controller, float deltaTime) override;
};

// 基準点から左方向(-X)へ歩く。patrolDistanceまで進んだらStateWalkRightへ切り替える。
class StateWalkLeft : public IEnemyState {
public:
	void Enter(EnemyStatusController* controller) override;
	void Update(EnemyStatusController* controller, float deltaTime) override;
};