#pragma once
#include "Application/Definitions/Character/Player/PlayerCombatTypes.h"
#include "../../../Tags/IHitReactionQuery.h"
#include "Application/Definitions/Character/Common/StateMachine/StateMachine.h"

#include "PlayerInputComponent.h"
#include "PlayerLockOnComponent.h"
#include "PlayerAttackSelector.h"
#include "PlayerMovementAnimationComponent.h"
#include "PlayerState.h"

#include "../Combat/WeaponSetComponent.h"
#include "../../../Graphics/Animation/ModelAnimatorComponent.h"

// ------------------------------------------------------------
// 前方宣言: 今回の責務分割で新設する兄弟コンポーネント群。
// PlayerStatusController自身はこれらの「公開インターフェース」だけを
// 知っていればよく、内部でどう計算しているかは知らない(知る必要がない)。
// 実体(.h本体・.cpp)は次のステップで個別に実装する。
// ------------------------------------------------------------
class PlayerAttackSelector;          // コンボ/技データの選択(PlayerAttackTable保持)
class PlayerFacingComponent;         // 攻撃対象・ロック対象への向き制御
class PlayerCombatMovementComponent; // 通常移動の速度適用＋攻撃/回避中の踏み込み移動

// ============================================================
// PlayerStatusController(責務再構成版)
//
// 【責務(最小要件)】
//  1. 戦闘State(IPlayerState)のFSMを保持し、遷移させること
//  2. 入力を受けて「今何を開始できるか」をStateに問い合わせ、
//     開始できるなら対応するCollaboratorへ実行を委譲すること
//  3. Stateが具体的なコンポーネント(ModelAnimatorComponent等)を
//     直接知らなくて済むよう、薄いファサードを提供すること
//
// 【今回のリファクタで外に出したもの】
//  - コンボ/技データの選択・保持(旧comboIndex_/comboAttacks_等)
//      → PlayerAttackSelector
//  - 攻撃対象・ロック対象への向き制御(旧FaceAttackTarget/FaceTowards/
//    UpdateLockOnFacing/currentAttackTarget_)
//      → PlayerFacingComponent
//  - 通常移動の速度適用＋攻撃/回避中の踏み込み移動(旧ApplyMovementState/
//    RequestStepMove系/walkSpeed_・runSpeed_)
//      → PlayerCombatMovementComponent
//
//  結果として、このController自身はTransformComponent/MovementComponent/
//  FacingDirectionComponentを直接持たなくなった(いずれも上記の
//  Collaborator側が内部で解決する)。Evade/Guardは分岐を持たない単純な
//  1件データのままのため、引き続きこのControllerが基礎データ
//  (baseEvadeData_/baseGuardData_)を保持する。
// ============================================================
class PlayerStatusController : public ComponentBase, public IHitReactionQuery
{
public:
	explicit PlayerStatusController(GameObject* owner) : ComponentBase(owner) {}

	void Start() override;
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

	// --- 現在実行中の技データ(Stateが毎フレーム参照する) ---
	// Attack分はattackSelector_が唯一の保持者。Controllerは中継するだけ。
	const AttackData& GetCurrentAttackData() const;
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

	// --- 武器の攻撃判定(WeaponSetComponentへの薄い委譲) ---
	void SetWeapon(Handle<WeaponComponent> weapon);
	void SetWeaponHitBoxEnabled(const std::vector<std::string>& slots, bool enabled);
	void SetWeaponTrailEmitting(const std::vector<std::string>& slots, bool emitting);

	// --- アニメーション再生(ModelAnimatorComponentへの薄い委譲) ---
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

private:
	void TransitionTo(IPlayerState* nextState);
	void ForceTransitionTo(IPlayerState* nextState);
	void OnStateChanged(IPlayerState* prevState, IPlayerState* nextState);

	void HandleMovementInput(const PlayerInputComponent& input, float deltaTime);
	void HandleActionInput(PlayerInputComponent& input);

	// --- 兄弟コンポーネント(既存) ---
	PlayerInputComponent* inputComponent_ = nullptr;
	PlayerLockOnComponent* lockOnComponent_ = nullptr;
	PlayerMovementAnimationComponent* movementAnimationComponent_ = nullptr;
	ModelAnimatorComponent* modelAnimatorComponent_ = nullptr;
	WeaponSetComponent* weaponSet_ = nullptr;

	// --- 兄弟コンポーネント(新設。骨格段階では前方宣言のみ) ---
	PlayerAttackSelector* attackSelector_ = nullptr;
	PlayerFacingComponent* facing_ = nullptr;
	PlayerCombatMovementComponent* combatMovement_ = nullptr;

	MovementState movementState_ = MovementState::Stand;

	// Evade/Guardは分岐を持たない単純な1件データのため、引き続き
	// このControllerが基礎データ・現在値を保持する(Attackのように
	// 専用のSelectorへ切り出すほどの選択ロジックが無いため)。
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
	static constexpr const char* kRootMotionBoneName = "root";
	static constexpr const char* kMainWeaponSlot = "Main";
};