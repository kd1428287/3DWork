#pragma once
#include "PlayerCombatTypes.h"
#include "PlayerInputComponent.h"
#include "../Movement/MovementComponent.h" // 既存の依存として
#include "PlayerState.h"

class PlayerStatusController : public ComponentBase
{
public:
	explicit PlayerStatusController(GameObject* owner) : ComponentBase(owner) {}

	void Start() override
	{
		inputComponent_ = GetOwner()->GetComponent<PlayerInputComponent>();
		movementComponent_ = GetOwner()->GetComponent<MovementComponent>();

		// 初期状態のセット
		currentState_ = &stateNone_;
	}

	// --- 移動軸: 参照 --------------------------------------------------
	MovementState GetMovementState() const { return movementState_; }

	// --- 戦闘軸: 参照 (現在のStateに委譲) ------------------------------
	CombatState GetCombatState() const { return currentState_->GetDetailedState(); }
	float GetCombatElapsed() const { return currentState_->GetElapsed(); }

	bool IsAttacking() const {
		auto state = GetCombatState();
		return state == CombatState::AttackWindup || state == CombatState::AttackActive || state == CombatState::AttackRecovery;
	}

	bool IsEvading() const {
		auto state = GetCombatState();
		return state == CombatState::Evade || state == CombatState::EvadeRecovery;
	}

	bool IsGuarding() const { return GetCombatState() == CombatState::Guard; }

	bool IsStaggered() const {
		auto state = GetCombatState();
		return state == CombatState::StaggerSmall || state == CombatState::StaggerLarge;
	}

	// ジャスト判定もState側に委譲
	bool IsInJustEvadeWindow() const { return currentState_->IsInJustEvadeWindow(this); }
	bool IsInParryWindow() const { return currentState_->IsInParryWindow(this); }

	// --- 戦闘軸: 実行可否 (現在のStateに委譲) --------------------------
	bool CanStartAttack() const { return currentState_->CanStartAttack(this); }
	bool CanStartEvade() const { return currentState_->CanStartEvade(this); }

	// --- データ取得 (Stateが判定に使うため) ----------------------------
	const AttackMoveData& GetCurrentAttackData() const { return currentAttack_; }
	const EvadeMoveData& GetCurrentEvadeData() const { return currentEvade_; }
	const GuardMoveData& GetCurrentGuardData() const { return currentGuard_; }

	// --- 状態遷移 (State内部から、あるいはControllerから呼ばれる) -------
	void ChangeStateToGuard() { TransitionTo(&stateGuard_); }
	void ChangeStateToNone() { TransitionTo(&stateNone_); }

	bool TryStartAttack(const AttackMoveData& move) {
		if (!CanStartAttack()) return false;
		currentAttack_ = move;
		TransitionTo(&stateAttack_);
		return true;
	}

	bool TryStartEvade(const EvadeMoveData& move) {
		if (!CanStartEvade()) return false;
		currentEvade_ = move;
		TransitionTo(&stateEvade_);
		return true;
	}

	void ApplyStagger(bool isLarge, float duration) {
		stateStagger_.Setup(isLarge, duration);
		// Staggerは同じStateインスタンスへの再突入がありうる(連続ヒット等)。
		// 通常のTransitionToは「同じインスタンスなら何もしない」ため、
		// 既に怯み中にもう一度怯みが発生するとEnter()が呼ばれずelapsed_が
		// リセットされない。ここだけは必ずExit→Enterし直す。
		ForceTransitionTo(&stateStagger_);
	}

	// --- ライフサイクル --------------------------------------------------
	void Update(float deltaTime) override
	{
		if (inputComponent_ != nullptr) {
			HandleMovementInput(*inputComponent_);
			HandleActionInput(*inputComponent_);
		}

		UpdateMovementState(deltaTime);

		// 戦闘状態の更新は現在のStateに丸投げ
		if (currentState_) {
			currentState_->Update(this, deltaTime);
		}
	}

private:
	void TransitionTo(IPlayerState* nextState) {
		if (currentState_ == nextState) return;
		ForceTransitionTo(nextState);
	}

	// currentState_ == nextStateであっても必ずExit→Enterし直す版。
	// Stagger等、同じStateインスタンスへの再突入で内部タイマーを
	// リセットしたい場合に使う。
	void ForceTransitionTo(IPlayerState* nextState) {
		if (currentState_) currentState_->Exit(this);
		currentState_ = nextState;
		currentState_->Enter(this);

		// 移動の許可/禁止はここに集約する。None以外(攻撃/回避/ガード/怯み)の
		// 間はMovementComponentごと無効化し、各Stateが個別に
		// enable/disableを気にしなくて済むようにする。
		if (movementComponent_) {
			movementComponent_->SetEnabled(nextState == &stateNone_);
		}
	}

	void HandleMovementInput(const PlayerInputComponent& input);
	void HandleActionInput(PlayerInputComponent& input);
	void ApplyMovementState(MovementState state);
	void UpdateMovementState(float deltaTime);

	// 兄弟コンポーネント
	PlayerInputComponent* inputComponent_ = nullptr;
	MovementComponent* movementComponent_ = nullptr;

	// --- 移動データ ---
	MovementState movementState_ = MovementState::Stand;
	float walkSpeed_ = 2.0f;
	float runSpeed_ = 5.0f;

	// --- 戦闘データ ---
	AttackMoveData currentAttack_;
	EvadeMoveData currentEvade_;
	GuardMoveData currentGuard_;

	// --- Stateインスタンス (メモリ断片化を防ぐため実体をメンバで持つ) ---
	StateNone    stateNone_;
	StateAttack  stateAttack_;
	StateEvade   stateEvade_;
	StateGuard   stateGuard_;
	StateStagger stateStagger_;

	IPlayerState* currentState_ = nullptr; // 現在アクティブなState
};