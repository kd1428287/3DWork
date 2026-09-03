// PlayerStatusController.cpp
#include "PlayerStatusController.h"
#include "../../Movement/TweenMoveComponent.h"
#include <algorithm> // std::min/std::max (RequestStepMoveTowardsTarget)

void PlayerStatusController::HandleMovementInput(const PlayerInputComponent& input)
{
	// 攻撃や回避中（None以外）は移動入力を無視する
	if (GetCombatState() != CombatState::None) return;
	MovementState nextState = input.GetDesiredMovementState();

	// Walk中は、Stand/Walk/Run間の切り替わりだけでなく、現在の向きに対する
	// 移動方向(前後左右)の変化もアニメーションを再生し直すきっかけにする。
	// ロックオン中は正面が移動方向に追従しなくなる(UpdateLockOnFacing参照)ため、
	// MovementState自体はWalkのまま前進⇔後退⇔横歩きが切り替わることがあり、
	// 従来の「MovementStateが変わった時だけPlay」する仕組みだけでは
	// この切り替わりを拾えない。
	const EvadeDirection walkDirection = ClassifyEvadeDirection(input.GetMoveDirection());
	const bool stateChanged = (movementState_ != nextState);
	const bool walkDirectionChanged = (nextState == MovementState::Walk && walkDirection != lastWalkDirection_);

	if (stateChanged) {
		movementState_ = nextState;
		ApplyMovementState(movementState_);
	}

	if (stateChanged || walkDirectionChanged) {
		lastWalkDirection_ = walkDirection;
		PlayMovementAnimation(movementState_, walkDirection);
	}
}

void PlayerStatusController::HandleActionInput(PlayerInputComponent& input)
{
	// スタン中(Stagger)は一切の行動入力を受け付けない。
	// Attack/Evadeの開始自体はCanStartAttack()/CanStartEvade()
	// (IPlayerStateのデフォルトfalse、StateStaggerは未オーバーライド)に
	// よっても防がれてはいるが、ここで早期リターンしておくことで、
	// HandleMovementInputと同じく「スタン中は行動入力を一切見ない」ことを
	// 明示し、将来Stateが増えた際にCanStart*系のオーバーライド漏れが
	// あってもスタン無敵貫通が起きないようにする。
	if (IsStaggered()) return;

	// ロックオンの切り替え。Attack/Evade/Guardのように現在のCombatStateで
	// 実行可否を制限する必要が無いため(戦闘中でも対象を切り替えたい/
	// 外したいことがある)、CanStart*系のポリモーフィズムには乗せず、
	// ここで直接処理する。既にロック中ならこの入力で解除、未ロックなら
	// 画面中心に最も近い対象をロックするトグル動作にしている。
	if (input.ConsumeLockPressed()) {
		if (IsLockedOn()) {
			ClearLockOn();
		}
		else {
			TryLockOn();
		}
	}

	if (input.IsGuardHeld()) {
		TryStartGuard();
	}
	else if (GetCombatState() == CombatState::Guard) {
		ChangeStateToNone(); // ガードキーを離したら即解除
	}

	if (input.HasCommand(ActionCommand::Evade) && CanStartEvade()) {
		EvadeMoveData data = baseEvadeData_;
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

void PlayerStatusController::RequestStepMoveTowardsTarget(const Math::Vector3& fallbackDirection, float stepDistance,
	float engageDistance, float duration)
{
	GameObject* target = currentAttackTarget_.Resolve();
	if (target == nullptr) {
		// 対象が見つからない(未ロック+画面中心付近に敵がいない等)場合は、
		// 決め打ち移動にフォールバックする。
		RequestStepMove(fallbackDirection, stepDistance, duration);
		return;
	}

	TransformComponent* targetTransform = target->GetComponent<TransformComponent>();
	TransformComponent* transform = GetOwner()->GetComponent<TransformComponent>();
	if (targetTransform == nullptr || transform == nullptr) {
		RequestStepMove(fallbackDirection, stepDistance, duration);
		return;
	}

	Math::Vector3 toTarget = targetTransform->GetPosition() - transform->GetPosition();
	toTarget.y = 0.0f;
	const float distanceToTarget = toTarget.Length();

	// 詰める距離 = 「今の距離からengageDistance分を残した距離」だが、
	// 一度の踏み込みで詰めてよい量はstepDistanceを上限とする
	const float closingDistance = std::min(stepDistance, std::max(0.0f, distanceToTarget - engageDistance));

	if (closingDistance <= kDirectionEpsilon) {
		return;
	}

	Math::Vector3 dir = toTarget;
	if (dir.LengthSquared() <= kDirectionEpsilon) {
		// 対象とほぼ同じ座標にいる(通常は起こらない想定)場合のみ、
		// モデルの向いている方向へフォールバックする(RequestStepMoveと同じ考え方)。
		dir = transform->GetForward();
	}
	else {
		dir.Normalize();
	}

	const Math::Vector3 from = transform->GetPosition();
	const Math::Vector3 to = from + dir * closingDistance;
	GetOwner()->RequestAddComponent<TweenMoveComponent>(from, to, duration);
}