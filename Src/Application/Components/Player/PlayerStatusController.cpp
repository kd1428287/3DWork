#include "PlayerStatusController.h"
#include "../Movement/TweenMoveComponent.h"

// =================================================================
// 各Stateの具体的なロジック実装
// =================================================================

// --- Attack State ---
void StateAttack::Enter(PlayerStatusController* controller) {
	phase_ = CombatState::AttackWindup;
	elapsed_ = 0.0f;

	GameObject* owner = controller->GetOwner();
	TransformComponent* transform = owner->GetComponent<TransformComponent>();
	const auto& data = controller->GetCurrentAttackData();

	if (transform != nullptr) {
		const Math::Vector3 from = transform->position;

		// 無入力(棒立ち)での回避は、モデルの向いている方向へフォールバックする。
		// (バッファ時点でゼロベクトルだった場合のみ。方向自体は既に
		// PlayerInputComponent::PushCommand側で正規化済み)
		Math::Vector3 dir = data.stepDirection;
		if (dir.LengthSquared() <= kDirectionEpsilon) {
			dir = transform->GetForward();
		}

		const Math::Vector3 to = from + dir * data.stepDistance;
		const float duration = data.windupDuration;

		owner->RequestAddComponent<TweenMoveComponent>(from, to, duration);
	}

	KdDebugGUI::Instance().AddLog("AttackWindup"); // 必要なら
}

void StateAttack::Update(PlayerStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;
	const auto& data = controller->GetCurrentAttackData();

	KdDebugGUI::Instance().AddLog("Attack"); 

	if (phase_ == CombatState::AttackWindup && elapsed_ >= data.windupDuration) {
		phase_ = CombatState::AttackActive;
		elapsed_ = 0.0f;
		controller->GetOwner()->RequestRemoveComponent<TweenMoveComponent>();
		KdDebugGUI::Instance().AddLog("\nAttackActive");
	}
	else if (phase_ == CombatState::AttackActive && elapsed_ >= data.activeDuration) {
		phase_ = CombatState::AttackRecovery;
		elapsed_ = 0.0f;
		KdDebugGUI::Instance().AddLog("\nAttackRecovery");
	}
	else if (phase_ == CombatState::AttackRecovery && elapsed_ >= data.recoveryDuration) {
		// 自律的に終了し、ControllerにNoneへの復帰を要請する
		controller->ChangeStateToNone();
	}
}

bool StateAttack::CanStartEvade(const PlayerStatusController* controller) const {
	if (phase_ == CombatState::AttackRecovery) {
		return elapsed_ >= controller->GetCurrentAttackData().recoveryEvadeCancelStart;
	}
	return false;
}

// --- Evade State ---
void StateEvade::Enter(PlayerStatusController* controller) {
	phase_ = CombatState::Evade;
	elapsed_ = 0.0f;
	KdDebugGUI::Instance().AddLog("Evade");

	// 回避中の移動は入力ではなく、決め打ちの軌道(TweenMoveComponent)に任せる。
	// MovementComponentはTransitionTo側で既に無効化されているため、
	// 位置を書き換える権利がここで競合することはない。
	GameObject* owner = controller->GetOwner();
	TransformComponent* transform = owner->GetComponent<TransformComponent>();
	const auto& data = controller->GetCurrentEvadeData();

	if (transform != nullptr) {
		const Math::Vector3 from = transform->position;

		// 無入力(棒立ち)での回避は、モデルの向いている方向へフォールバックする。
		// (バッファ時点でゼロベクトルだった場合のみ。方向自体は既に
		// PlayerInputComponent::PushCommand側で正規化済み)
		Math::Vector3 dir = data.evadeDirection;
		if (dir.LengthSquared() <= kDirectionEpsilon) {
			dir = transform->GetForward();
		}

		const Math::Vector3 to = from + dir * data.evadeDistance;
		const float duration = data.activeDuration + data.recoveryDuration;

		owner->RequestAddComponent<TweenMoveComponent>(from, to, duration);
	}
}

void StateEvade::Exit(PlayerStatusController* controller) {
	// EvadeRecovery終了(あるいは何らかの理由での中断)で必ず後始末する。
	controller->GetOwner()->RequestRemoveComponent<TweenMoveComponent>();
}

void StateEvade::Update(PlayerStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;
	const auto& data = controller->GetCurrentEvadeData();
	KdDebugGUI::Instance().AddLog("Evade");
	if (phase_ == CombatState::Evade && elapsed_ >= data.activeDuration) {
		phase_ = CombatState::EvadeRecovery;
		elapsed_ = 0.0f;

	}
	else if (phase_ == CombatState::EvadeRecovery && elapsed_ >= data.recoveryDuration) {
		controller->ChangeStateToNone();
		KdDebugGUI::Instance().AddLog("\nEvadeRecovery");
	}
}

bool StateEvade::IsInJustEvadeWindow(const PlayerStatusController* controller) const {
	if (phase_ != CombatState::Evade) return false;
	const auto& data = controller->GetCurrentEvadeData();
	return elapsed_ >= data.justWindowStart && elapsed_ <= data.justWindowEnd;
}

// --- Guard State ---
void StateGuard::Enter(PlayerStatusController* controller) {
	elapsed_ = 0.0f;
	KdDebugGUI::Instance().AddLog("Guard");
}

void StateGuard::Update(PlayerStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;
	// Guardは継続状態なので、時間経過による自動終了はない
}

bool StateGuard::IsInParryWindow(const PlayerStatusController* controller) const {
	return elapsed_ <= controller->GetCurrentGuardData().justWindowDuration;
}

// --- Stagger State ---
void StateStagger::Enter(PlayerStatusController* controller) {
	elapsed_ = 0.0f;
	KdDebugGUI::Instance().AddLog("Stagger");
}

void StateStagger::Update(PlayerStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;
	if (elapsed_ >= duration_) {
		controller->ChangeStateToNone();
	}
}

// =================================================================
// PlayerStatusController 本体の実装
// =================================================================

void PlayerStatusController::HandleMovementInput(const PlayerInputComponent& input)
{
	// 攻撃や回避中（None以外）は移動入力を無視する
	if (GetCombatState() != CombatState::None) return;
	MovementState nextState = input.GetDesiredMovementState();

	if (movementState_ != nextState) {
		movementState_ = nextState;
		ApplyMovementState(movementState_);
	}
}

void PlayerStatusController::HandleActionInput(PlayerInputComponent& input)
{
	if (input.IsGuardHeld()) {
		if (GetCombatState() == CombatState::None) {
			currentGuard_ = GuardMoveData{};
			ChangeStateToGuard();
		}
	}
	else if (GetCombatState() == CombatState::Guard) {
		ChangeStateToNone(); // ガードキーを離したら即解除
	}

	if (input.HasCommand(ActionCommand::Evade) && CanStartEvade()) {
		EvadeMoveData data{};
		// 積まれた瞬間の方向スナップショットをそのまま使う
		// (消費するこのフレームの生入力ではなく)。
		input.ConsumeCommand(ActionCommand::Evade, data.evadeDirection);
		TryStartEvade(data);
	}
	else if (input.HasCommand(ActionCommand::Attack) && CanStartAttack()) {
		AttackMoveData data{};
		input.ConsumeCommand(ActionCommand::Attack, data.stepDirection);
		TryStartAttack(data);
	}
}

void PlayerStatusController::ApplyMovementState(MovementState state)
{
	if (!movementComponent_) return;
	switch (state) {
	case MovementState::Stand: break;
	case MovementState::Walk: movementComponent_->SetSpeed(walkSpeed_); break;
	case MovementState::Run:  movementComponent_->SetSpeed(runSpeed_); break;
	}
}

void PlayerStatusController::UpdateMovementState(float deltaTime)
{
	if (GetCombatState() != CombatState::None) return;

	if (movementState_ == MovementState::Run) {
		// スタミナ消費処理用スペース
	}
}