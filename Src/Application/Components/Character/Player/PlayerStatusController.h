#pragma once
#include "PlayerCombatTypes.h"
#include "PlayerInputComponent.h"
#include "../../Movement/MovementComponent.h" // 既存の依存として
#include "PlayerState.h"
#include "../StateMachine/StateMachine.h"

class PlayerStatusController : public ComponentBase
{
public:
	explicit PlayerStatusController(GameObject* owner) : ComponentBase(owner) {}

	void Start() override
	{
		inputComponent_ = GetOwner()->GetComponent<PlayerInputComponent>();
		movementComponent_ = GetOwner()->GetComponent<MovementComponent>();

		// 初期状態のセット。TransitionTo経由なのでEnterも呼ばれるが、
		// StateNone::Enterは何もしないため実質は代入と同じ。
		TransitionTo(&stateNone_);
	}

	// --- 移動軸: 参照 --------------------------------------------------
	MovementState GetMovementState() const { return movementState_; }

	// --- 戦闘軸: 参照 (現在のStateに委譲) ------------------------------
	CombatState GetCombatState() const { return stateMachine_.Current()->GetDetailedState(); }
	float GetCombatElapsed() const { return stateMachine_.Current()->GetElapsed(); }

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
	bool IsInJustEvadeWindow() const { return stateMachine_.Current()->IsInJustEvadeWindow(this); }
	bool IsInParryWindow() const { return stateMachine_.Current()->IsInParryWindow(this); }

	// --- 戦闘軸: 実行可否 (現在のStateに委譲) --------------------------
	// Guardもここに揃える(以前はHandleActionInput内でCombatState::Noneを
	// 直接比較していたが、Attack/Evadeと同じくState側のポリモーフィズムに乗せる)。
	bool CanStartAttack() const { return stateMachine_.Current()->CanStartAttack(this); }
	bool CanStartEvade() const { return stateMachine_.Current()->CanStartEvade(this); }
	bool CanStartGuard() const { return stateMachine_.Current()->CanStartGuard(this); }

	// --- データ取得 (Stateが判定に使うため) ----------------------------
	const AttackMoveData& GetCurrentAttackData() const { return currentAttack_; }
	const EvadeMoveData& GetCurrentEvadeData() const { return currentEvade_; }
	const GuardMoveData& GetCurrentGuardData() const { return currentGuard_; }

	// --- 状態遷移 (State内部から、あるいはControllerから呼ばれる) -------
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

	bool TryStartGuard() {
		if (!CanStartGuard()) return false;
		currentGuard_ = GuardMoveData{};
		TransitionTo(&stateGuard_);
		return true;
	}

	void ApplyStagger(bool isLarge, float duration) {
		stateStagger_.Setup(isLarge, duration);
		// Staggerは同じStateインスタンスへの再突入がありうる(連続ヒット等)。
		// 通常のTransitionToは「同じインスタンスなら何もしない」ため、
		// ForceTransitionToで必ずExit→Enterし直す。
		stateMachine_.ForceTransitionTo(this, &stateStagger_);
		OnStateChanged(&stateStagger_);
	}

	// --- Stateからの移動リクエスト --------------------------------------
	// 現在位置から指定方向へdistanceだけduration秒かけて移動する。
	// directionがほぼゼロベクトルの場合はモデルの向いている方向へ
	// フォールバックする。具体的なコンポーネント実装(TweenMoveComponent等)は
	// ここに閉じ込め、StateはTransformComponent/TweenMoveComponentを
	// 直接知らなくて済むようにする。
	void RequestStepMove(const Math::Vector3& direction, float distance, float duration);

	// 進行中のステップ移動があれば止める(フェーズ遷移や状態の中断時に使う)。
	void CancelStepMove();

	// --- ライフサイクル --------------------------------------------------
	void Update(float deltaTime) override
	{
		if (inputComponent_ != nullptr) {
			HandleMovementInput(*inputComponent_);
			HandleActionInput(*inputComponent_);
		}

		UpdateMovementState(deltaTime);

		// 戦闘状態の更新は共通StateMachineに丸投げ
		stateMachine_.Update(this, deltaTime);
	}

private:
	void TransitionTo(IPlayerState* nextState) {
		if (stateMachine_.TransitionTo(this, nextState)) {
			OnStateChanged(nextState);
		}
	}

	// 移動の許可/禁止はここに集約する。None以外(攻撃/回避/ガード/怯み)の
	// 間はMovementComponentごと無効化し、各Stateが個別に
	// enable/disableを気にしなくて済むようにする。
	void OnStateChanged(IPlayerState* nextState) {
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

	// 共通StateMachine基盤(遷移ロジック・現在Stateの保持)
	StateMachine<PlayerStatusController, IPlayerState> stateMachine_;
};
