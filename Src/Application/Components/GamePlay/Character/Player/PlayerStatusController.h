// PlayerStatusController.h
#pragma once
#include "Application/Definitions/Character/Player/PlayerCombatTypes.h"
#include "../../../Tags/IHitReactionQuery.h"
#include "Application/Definitions/Character/Common/StateMachine/StateMachine.h"

#include "PlayerInputComponent.h"
#include "PlayerLockOnComponent.h"
#include "PlayerMovementAnimationComponent.h"
#include "PlayerState.h"
#include "../Common/CharacterInputBufferComponent.h"

#include "../Common/WeaponSetComponent.h"
#include "../../../Graphics/Animation/ModelAnimatorComponent.h"

class PlayerAttackSelector;
class PlayerFacingComponent;
class PlayerCombatMovementComponent;

// PlayerStatusController(責務再構成版)

// 【責務(最小要件)】
//  1. 戦闘State(IPlayerState)のFSMを保持し、遷移させること
//  2. 入力を受けて「今何を開始できるか」をStateに問い合わせ、
//     開始できるなら対応するCollaboratorへ実行を委譲すること
//  3. Stateが具体的なコンポーネント(ModelAnimatorComponent等)を
//     直接知らなくて済むよう、薄いファサードを提供すること

class PlayerStatusController : public ComponentBase, public IHitReactionQuery
{
public:
	explicit PlayerStatusController(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override;
	void Update(float deltaTime) override;

	// --- 移動軸: 参照 ---
	MovementState GetMovementState() const { return movementState_; }

	// --- 戦闘軸: 参照(現在のStateへ委譲するだけ) ---
	CombatState GetCombatState() const { return stateMachine_.Current()->GetDetailedState(); }
	float GetCombatElapsed() const { return stateMachine_.Current()->GetElapsed(); }

	bool IsAttacking() const { return GetCombatState() == CombatState::AttackWindup || GetCombatState() == CombatState::AttackActive || GetCombatState() == CombatState::AttackRecovery; }
	bool IsEvading() const { return GetCombatState() == CombatState::Evade || GetCombatState() == CombatState::EvadeRecovery; }
	bool IsStaggered() const { return GetCombatState() == CombatState::StaggerSmall || GetCombatState() == CombatState::StaggerLarge; }
	bool IsGuarding() const override { return GetCombatState() == CombatState::Guard; }

	bool IsInJustEvadeWindow() const { return stateMachine_.Current()->IsInJustEvadeWindow(this); }
	bool IsInParryWindow() const override { return stateMachine_.Current()->IsInParryWindow(this); }
	bool IsInvincible() const { return stateMachine_.Current()->IsInvincible(this); }

	// --- 戦闘軸: 実行可否(現在のStateへ委譲するだけ) ---
	bool CanStartAttack() const { return stateMachine_.Current()->CanStartAttack(this); }
	bool CanStartEvade() const { return stateMachine_.Current()->CanStartEvade(this); }
	bool CanStartGuard() const { return stateMachine_.Current()->CanStartGuard(this); }
	bool CanReleaseGuard() const { return stateMachine_.Current()->CanReleaseGuard(this); }

	// Recovery中などに移動入力でキャンセルし、通常移動(None)へ復帰して
	// よいか。HandleMovementInputから使う(CanStartAttack等と同じ位置づけ)。
	bool CanStartMove() const { return stateMachine_.Current()->CanStartMove(this); }

	// --- 現在実行中の技データ(Stateが毎フレーム参照する) ---
	const EvadeData& GetCurrentEvadeData() const { return currentEvade_; }
	const GuardData& GetCurrentGuardData() const { return currentGuard_; }

	// 回避方向の分類。現在の向き(Transform)を必要とするためfacing_へ委譲する。
	EvadeDirection ClassifyEvadeDirection(const Math::Vector3& inputDirection) const;

	// --- 状態遷移(Stateからの要求の受け口) ---
	void ChangeStateToNone() { TransitionTo(&stateNone_); }
	bool TryStartAttack();
	bool TryStartEvade(const EvadeData& data);
	bool TryStartGuard();
	void ApplyStagger(bool isLarge, float duration);

	// --- IHitReactionQuery ---
	void EnterStagger(bool isLarge, float duration) override { ApplyStagger(isLarge, duration); }
	void NotifyParrySuccess() override;
	void NotifyGuardHit() override;

	// --- ロックオン(PlayerLockOnComponentへの薄い委譲) ---
	void TryLockOn();
	void ClearLockOn();
	bool IsLockedOn() const;

	// --- 攻撃対象への正対(PlayerFacingComponentへの薄い委譲) ---
	void FaceAttackTarget();

	// --- 武器の装備登録(WeaponSetComponentへの薄い委譲) ---
	// ヒットボックス/トレイルのon-offはStateAttackのみが使うため、
	// StateAttackがWeaponSetComponentを直接保持して呼ぶ(こちらの
	// 登録処理はPlayerFactory等、Stateの外から呼ばれるため引き続き
	// Controllerのファサードとして残す)。
	void SetWeapon(Handle<WeaponComponent> weapon);

	// PlayerFactory構築時にEvade/Guardデータを注入するためのセッター
	void SetEvadeAndGuardData(const EvadeData& evade, const GuardData& guard)
	{
		baseEvadeData_ = evade;
		baseGuardData_ = guard;
	}

	// --- アニメーション再生(ModelAnimatorComponentへの薄い委譲) ---
	void PlayAnimation(const std::string& name, bool loop, float targetDurationSeconds,
		float startTime, float endTime,
		bool useRootMotion, float blendDurationSeconds);

	void PlayAnimation(const std::string& name, bool loop = false, float targetDurationSeconds = -1.0f,
		bool useRootMotion = false, float blendDurationSeconds = kDefaultAnimationBlendDuration);

	// 戦闘行動からNoneへ復帰した直後、現在の入力状態に合わせて
	// Idle/Walk/Runへアニメーション・向きを同期し直す(StateNone::Enter参照)。
	void RefreshMovementAnimation();

	// --- Stateからの移動リクエスト(PlayerCombatMovementComponentへの委譲) ---
	void RequestStepMove(const Math::Vector3& direction, float distance, float duration);
	void RequestStepMoveTowardsTarget(const Math::Vector3& fallbackDirection, float stepDistance,
		float engageDistance, float duration);
	void CancelStepMove();
	void SetMovementEnabled(bool enabled);

private:
	void TransitionTo(IPlayerState* nextState);
	void ForceTransitionTo(IPlayerState* nextState);
	void OnStateChanged(IPlayerState* prevState, IPlayerState* nextState);

	void HandleMovementInput(const PlayerInputComponent& input, float deltaTime);
	void HandleActionInput(PlayerInputComponent& input);

	// --- 兄弟コンポーネント ---
	PlayerInputComponent* inputComponent_ = nullptr;
	PlayerLockOnComponent* lockOnComponent_ = nullptr;
	PlayerMovementAnimationComponent* movementAnimationComponent_ = nullptr;
	ModelAnimatorComponent* modelAnimatorComponent_ = nullptr;
	WeaponSetComponent* weaponSet_ = nullptr;
	PlayerAttackSelector* attackSelector_ = nullptr;
	PlayerFacingComponent* facing_ = nullptr;
	PlayerCombatMovementComponent* combatMovement_ = nullptr;

	MovementState movementState_ = MovementState::Stand;

	EvadeData baseEvadeData_;
	EvadeData currentEvade_;
	GuardData baseGuardData_;
	GuardData currentGuard_;

	// --- Stateインスタンス ---
	StateNone    stateNone_;
	StateAttack  stateAttack_;
	StateEvade   stateEvade_;
	StateGuard   stateGuard_;
	StateStagger stateStagger_;

	StateMachine<PlayerStatusController, IPlayerState> stateMachine_;

	static constexpr float kDefaultAnimationBlendDuration = 0.15f;
	static constexpr const char* kRootMotionBoneName = "mixamorig_Hips";
	static constexpr const char* kMainWeaponSlot = "Main";
};