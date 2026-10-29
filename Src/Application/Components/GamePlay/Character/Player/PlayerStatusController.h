// PlayerStatusController.h
#pragma once
#include "Application/Definitions/Character/Player/PlayerCombatTypes.h"
#include "../../../Tags/IHitReactionQuery.h"
#include "Application/Definitions/Character/Common/StateMachine/StateMachine.h"

#include "PlayerInputComponent.h"
#include "PlayerLockOnComponent.h"
#include "PlayerMovementAnimationComponent.h"
#include "PlayerState.h"
#include "PlayerStateContext.h"
#include "../Common/CharacterInputBufferComponent.h"

#include "../Common/WeaponSetComponent.h"
#include "../../../Graphics/Animation/ModelAnimatorComponent.h"

class PlayerAttackSelector;
class PlayerFacingComponent;
class PlayerCombatMovementComponent;
class HealthComponent;

// 初期設定(Prefab/JSONから渡す値)。SetConfigで各Stateへ配る。
struct PlayerStatusControllerConfig
{
	EvadeData evade;
	GuardData guard;
	ChargeData charge;
	StaggerData stagger;
	AttackStateData attack;
	DeadData dead;
};

// PlayerStatusController(責務再構成版)

// 【責務(最小要件)】
//  1. 戦闘State(IPlayerState)のFSMを保持し、遷移させること
//  2. 入力を受けて「今何を開始できるか」をStateに問い合わせ、
//     開始できるなら対応するCollaboratorへ実行を委譲すること
//  3. Stateが依存するコンポーネントをPlayerStateContextにまとめて渡すこと

class PlayerStatusController : public ComponentBase, public IHitReactionQuery, private IPlayerStateRequester
{
public:
	using Config = PlayerStatusControllerConfig;

	explicit PlayerStatusController(GameObject* owner) : ComponentBase(owner) {}

	void SetConfig(const Config& config)
	{
		stateEvade_.SetData(config.evade);
		stateGuard_.SetData(config.guard);
		stateCharge_.SetData(config.charge);
		stateStagger_.SetData(config.stagger);
		stateAttack_.SetData(config.attack);
		stateDead_.SetData(config.dead);
	}

	void Awake() override;
	void Update(float deltaTime) override;

	// --- 移動軸: 参照 ---
	MovementState GetMovementState() const { return ctx_.movementState; }

	// --- 戦闘軸: 参照(現在のStateへ委譲するだけ) ---
	CombatState GetCombatState() const { return stateMachine_.Current()->GetDetailedState(); }
	float GetCombatElapsed() const { return stateMachine_.Current()->GetElapsed(); }

	bool IsAttacking() const { return GetCombatState() == CombatState::AttackWindup || GetCombatState() == CombatState::AttackActive || GetCombatState() == CombatState::AttackRecovery; }
	bool IsEvading() const { return GetCombatState() == CombatState::Evade || GetCombatState() == CombatState::EvadeRecovery; }
	bool IsStaggered() const { return GetCombatState() == CombatState::StaggerSmall || GetCombatState() == CombatState::StaggerLarge; }
	bool IsGuarding() const override { return GetCombatState() == CombatState::Guard; }
	bool IsCharging() const { return GetCombatState() == CombatState::Charge; }
	bool IsDead() const { return GetCombatState() == CombatState::Dead; }

	bool IsInJustEvadeWindow() const { return stateMachine_.Current()->IsInJustEvadeWindow(&ctx_); }
	bool IsInParryWindow() const override { return stateMachine_.Current()->IsInParryWindow(&ctx_); }
	bool IsInvincible() const { return stateMachine_.Current()->IsInvincible(&ctx_); }

	// --- 戦闘軸: 実行可否(現在のStateへ委譲するだけ) ---
	bool CanStartAttack() const { return stateMachine_.Current()->CanStartAttack(&ctx_); }
	bool CanStartEvade() const { return stateMachine_.Current()->CanStartEvade(&ctx_); }
	bool CanStartGuard() const { return stateMachine_.Current()->CanStartGuard(&ctx_); }
	bool CanReleaseGuard() const { return stateMachine_.Current()->CanReleaseGuard(&ctx_); }

	// Recovery中などに移動入力でキャンセルし、通常移動(None)へ復帰して
	// よいか。HandleMovementInputから使う(CanStartAttack等と同じ位置づけ)。
	bool CanStartMove() const { return stateMachine_.Current()->CanStartMove(&ctx_); }

	// --- 状態遷移(Stateからの要求の受け口) ---
	void ChangeStateToNone() override { TransitionTo(&stateNone_); }
	bool TryStartAttack();
	bool TryStartEvade(const Math::Vector3& direction);
	bool TryStartGuard();
	bool TryStartCharge();

	// 溜めを解放してチャージ攻撃を発射する。HandleActionInputから呼ぶ。
	void ReleaseCharge();
	void ApplyStagger(bool isLarge);

	// HPが0になった際に死亡Stateへ強制遷移する(一度入ったら抜けない)。
	void Die();

	// --- IHitReactionQuery ---
	void EnterStagger(bool isLarge) override { ApplyStagger(isLarge); }
	void NotifyParrySuccess() override;
	void NotifyGuardHit() override;

	// ガードキーが離された際にHandleActionInputから呼ばれる。即座にNoneへは
	// 戻らず、StateGuardに終了アニメーションの再生を開始させるだけ
	// (完了後はStateGuard::Update()が自律的にChangeStateToNone()する)。
	void RequestGuardRelease();

	// --- ロックオン(PlayerLockOnComponentへの薄い委譲) ---
	void TryLockOn();
	void ClearLockOn();
	bool IsLockedOn() const;

	// --- 武器の装備登録(PlayerFactory等、Stateの外から呼ぶ用) ---
	void SetWeapon(Handle<WeaponComponent> weapon);

	// 既存呼び出し互換用。各Stateへデータを渡すだけ。
	void SetEvadeAndGuardData(const EvadeData& evade, const GuardData& guard)
	{
		stateEvade_.SetData(evade);
		stateGuard_.SetData(guard);
	}

private:
	void TransitionTo(IPlayerState* nextState);
	void ForceTransitionTo(IPlayerState* nextState);
	void OnStateChanged(IPlayerState* prevState, IPlayerState* nextState);

	void HandleMovementInput(const PlayerInputComponent& input, float deltaTime);
	void HandleActionInput(PlayerInputComponent& input);

	// Stateが参照するコンポーネント束と共有状態(movementState含む)。
	PlayerStateContext ctx_;
	HealthComponent* health_ = nullptr;

	// --- Stateインスタンス ---
	StateNone    stateNone_;
	StateAttack  stateAttack_;
	StateEvade   stateEvade_;
	StateGuard   stateGuard_;
	StateCharge  stateCharge_;
	StateStagger stateStagger_;
	StateDead    stateDead_;

	StateMachine<PlayerStateContext, IPlayerState> stateMachine_;

	static constexpr const char* kMainWeaponSlot = "Main";
};