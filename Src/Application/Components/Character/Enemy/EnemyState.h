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

// HurtBoxがHitBoxを受けた際に、攻撃側(AttackSourceComponent)から
// StateKnockbackへ渡すパラメータ一式。
//
// 実際の吹っ飛び移動自体はVelocityComponent::AddImpulse()に委譲する
// (StateKnockback::Enter参照)。以前はMovementComponent経由で毎フレーム
// 手動のdesiredDirection_を設定していたが、これだとVelocityComponent
// (重力)やCollisionSystem(地面の押し返し)と互いに一切協調せず、
// 「地面へ潜り込む方向の速度を毎フレーム強制上書き→押し戻される→
// また上書き」を繰り返して地面にめり込んだ状態になる不具合があった。
// VelocityComponentのimpulseVelocity_チャンネルに統一することで、
// 重力との合算・MovementComponentの自動一時停止(IsImpulseActive()参照)・
// 摩擦減衰のすべてを既存の仕組みにそのまま任せられる。
struct KnockbackParams {
	Math::Vector3 direction{}; // 吹っ飛ぶ向き(正規化済み想定)
	float power = 0.0f;        // 初速の大きさ

	// 怯みの「最低保証時間」(秒)。VelocityComponentの摩擦減衰だけで
	// 終了判定すると、power次第では一瞬でIsImpulseActive()==falseに
	// なってしまい、演出として不自然に短い怯みになりうる。実際の終了は
	// 「この時間が経過していて、かつ物理的にも止まっている」の両方を
	// 満たした時点(StateKnockback::Update参照)。
	float minStunDuration = 0.0f;
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

class StateKnockback : public IEnemyState {
public:
	// 遷移前に呼ぶ。TransitionTo/ForceTransitionToはStatePtrしか渡せないため、
	// 引数はEnter前にここでセットしておく(EnemyStatusController::
	// ChangeStateToKnockback参照)。
	void SetParams(const KnockbackParams& params) { params_ = params; }

	void Enter(EnemyStatusController* controller) override;
	void Update(EnemyStatusController* controller, float deltaTime) override;
	void Exit(EnemyStatusController* controller) override;

private:
	KnockbackParams params_;
	float elapsed_ = 0.0f;
};