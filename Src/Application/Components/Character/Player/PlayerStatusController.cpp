// PlayerStatusController.cpp
#include "PlayerStatusController.h"
#include "../../Movement/TweenMoveComponent.h"

void PlayerStatusController::HandleMovementInput(const PlayerInputComponent& input)
{
	// 攻撃や回避中（None以外）は移動入力を無視する
	if (GetCombatState() != CombatState::None) return;
	MovementState nextState = input.GetDesiredMovementState();

	if (movementState_ != nextState) {
		movementState_ = nextState;
		ApplyMovementState(movementState_);
		PlayMovementAnimation(movementState_);
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
		// コンボ何段目の技データを使うかはController内部(comboIndex_)が
		// 判断するため、消費するのはコマンドの存在だけでよい。
		input.ConsumeCommand(ActionCommand::Attack);
		TryStartAttack();
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