#include "PlayerStatusController.h"
#include "../../Movement/TweenMoveComponent.h"

// =================================================================
// 各Stateの具体的なロジック実装
// =================================================================

// --- Attack State ---
void StateAttack::Enter(PlayerStatusController* controller) {
	phase_ = CombatState::AttackWindup;
	elapsed_ = 0.0f;

	// 具体的なコンポーネント操作(Transform/TweenMoveComponent)は
	// Controller側のRequestStepMoveに閉じ込め、Stateはそれを
	// 直接知らなくてよいようにする。
	const auto& data = controller->GetCurrentAttackData();
	controller->RequestStepMove(data.stepDirection, data.stepDistance, data.windupDuration);

	KdDebugGUI::Instance().AddLog("AttackWindup"); // 必要なら
}

void StateAttack::Update(PlayerStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;
	const auto& data = controller->GetCurrentAttackData();

	KdDebugGUI::Instance().AddLog("Attack"); 

	if (phase_ == CombatState::AttackWindup && elapsed_ >= data.windupDuration) {
		phase_ = CombatState::AttackActive;
		elapsed_ = 0.0f;
		controller->CancelStepMove();
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

void StateAttack::Exit(PlayerStatusController* controller) {
	// Windup中にStagger等で強制的に割り込まれた場合など、通常のUpdateの
	// 遷移では回収できないタイミングでもステップ移動が残らないよう、
	// Exitで必ず後始末する(Evadeと同じ考え方)。
	controller->CancelStepMove();
}

bool StateAttack::CanStartEvade(const PlayerStatusController* controller) const {
	if (phase_ == CombatState::AttackRecovery) {
		return elapsed_ >= controller->GetCurrentAttackData().recoveryEvadeCancelStart;
	}
	return false;
}

bool StateAttack::CanStartAttack(const PlayerStatusController* controller) const {
	// Recovery中の一定タイミングを過ぎたら、次の攻撃(コンボ)への
	// キャンセルを許可する。CanStartEvadeと同じ考え方。
	if (phase_ == CombatState::AttackRecovery) {
		return elapsed_ >= controller->GetCurrentAttackData().recoveryAttackCancelStart;
	}
	return false;
}

// --- Evade State ---
void StateEvade::Enter(PlayerStatusController* controller) {
	phase_ = CombatState::Evade;
	elapsed_ = 0.0f;
	KdDebugGUI::Instance().AddLog("Evade");

	// 回避中の移動は入力ではなく、決め打ちの軌道(RequestStepMove)に任せる。
	// MovementComponentはTransitionTo側で既に無効化されているため、
	// 位置を書き換える権利がここで競合することはない。
	const auto& data = controller->GetCurrentEvadeData();
	controller->RequestStepMove(data.evadeDirection, data.evadeDistance, data.activeDuration + data.recoveryDuration);
}

void StateEvade::Exit(PlayerStatusController* controller) {
	// EvadeRecovery終了(あるいは何らかの理由での中断)で必ず後始末する。
	controller->CancelStepMove();
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
	return GetGuardPhase(controller) == GuardPhase::JustWindow;
}

StateGuard::GuardPhase StateGuard::GetGuardPhase(const PlayerStatusController* controller) const {
	return elapsed_ <= controller->GetCurrentGuardData().justWindowDuration
		? GuardPhase::JustWindow
		: GuardPhase::NormalBlock;
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
		modelAnimatorComponent_->Play("mixamo.com", true);
	}
}

void PlayerStatusController::HandleActionInput(PlayerInputComponent& input)
{
	// Guardの開始可否もAttack/Evadeと同じくCanStartGuard()/TryStartGuard()
	// (=State側のポリモーフィズム)に委ねる。CombatState::Noneのハードコード
	// 比較はここには置かない。
	if (input.IsGuardHeld()) {
		TryStartGuard();
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

void PlayerStatusController::RequestStepMove(const Math::Vector3& direction, float distance, float duration)
{
	GameObject* owner = GetOwner();
	TransformComponent* transform = owner->GetComponent<TransformComponent>();
	if (transform == nullptr) return;

	// 無入力(棒立ち)での要求は、モデルの向いている方向へフォールバックする。
	// (方向自体は呼び出し元でPlayerInputComponent::PushCommand時点で
	// 正規化済みであることを前提とする)
	Math::Vector3 dir = direction;
	if (dir.LengthSquared() <= kDirectionEpsilon) {
		dir = transform->GetForward();
	}

	const Math::Vector3 from = transform->GetPosition();
	const Math::Vector3 to = from + dir * distance;
	owner->RequestAddComponent<TweenMoveComponent>(from, to, duration);
}

void PlayerStatusController::CancelStepMove()
{
	GetOwner()->RequestRemoveComponent<TweenMoveComponent>();
}
