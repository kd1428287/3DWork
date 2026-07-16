#pragma once

// 前方宣言
class EnemyStatusController;

// ============================================================
// IEnemyState
// PlayerState(IPlayerState)と同じ、軽量Stateパターンの基底インターフェース。
// GameObjectにはアタッチせず、EnemyStatusController内部で
// インスタンスとして保持される。
// ============================================================
class IEnemyState {
public:
	virtual ~IEnemyState() = default;

	virtual void Enter(EnemyStatusController* controller) {}
	virtual void Update(EnemyStatusController* controller, float deltaTime) {}
	virtual void Exit(EnemyStatusController* controller) {}
};

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