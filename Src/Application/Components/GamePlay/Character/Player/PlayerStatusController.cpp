// PlayerStatusController.cpp
#include "PlayerStatusController.h"

#include "PlayerAttackSelector.h"
#include "PlayerFacingComponent.h"
#include "PlayerCombatMovementComponent.h"

#include "../Common/HealthComponent.h" // パスはプロジェクトの配置に合わせて調整
#include "../Common/HitReactionComponent.h"
#include "../../../Physics/Collision/ColliderComponent.h"
#include <cassert>

void PlayerStatusController::Awake()
{
	ctx_.input = GetOwner()->GetComponent<PlayerInputComponent>();
	ctx_.lockOn = GetOwner()->GetComponent<PlayerLockOnComponent>();
	ctx_.movementAnimation = GetOwner()->GetComponent<PlayerMovementAnimationComponent>();
	ctx_.animator = GetOwner()->GetComponent<ModelAnimatorComponent>();
	ctx_.weaponSet = GetOwner()->GetComponent<WeaponSetComponent>();
	ctx_.attackSelector = GetOwner()->GetComponent<PlayerAttackSelector>();
	ctx_.facing = GetOwner()->GetComponent<PlayerFacingComponent>();
	ctx_.combatMovement = GetOwner()->GetComponent<PlayerCombatMovementComponent>();
	ctx_.requester = this;
	health_ = GetOwner()->GetComponent<HealthComponent>();

	// Stateが前提とするコンポーネント。欠けていれば構成ミス。
	assert(ctx_.animator && ctx_.combatMovement && ctx_.facing && ctx_.lockOn && ctx_.weaponSet && ctx_.attackSelector);

	// 自分自身をIHitReactionQueryとして登録
	if (HitReactionComponent* hitReaction = GetOwner()->GetComponent<HitReactionComponent>()) {
		hitReaction->SetQuerySource(this);
		if (ctx_.weaponSet != nullptr) {
			if (WeaponComponent* mainWeapon = ctx_.weaponSet->GetWeapon(kMainWeaponSlot)) {
				hitReaction->SetWeapon(Handle<WeaponComponent>(mainWeapon));
			}
		}
	}

	// 初期状態のセット
	TransitionTo(&stateNone_);
}

void PlayerStatusController::Update(float deltaTime)
{
	// 死亡検知。Stateは一度入ったら抜けない。
	if (!IsDead() && health_ != nullptr && health_->IsDead()) {
		Die();
	}
	if (IsDead()) {
		stateMachine_.Update(&ctx_, deltaTime);
		return; // 入力・移動処理は一切行わない
	}

	if (ctx_.input != nullptr) {
		HandleMovementInput(*ctx_.input, deltaTime);
		HandleActionInput(*ctx_.input);
	}

	// ロック中の向き固定・移動状態の更新は、いずれもCombatState::Noneの間だけ行う
	if (GetCombatState() == CombatState::None) {
		if (ctx_.facing != nullptr) {
			ctx_.facing->UpdateLockOnFacing(ctx_.movementState == MovementState::Run);
		}
		if (ctx_.combatMovement != nullptr) {
			ctx_.combatMovement->UpdateMovementState(ctx_.movementState, deltaTime);
		}
	}

	// 戦闘状態の更新は共通StateMachineに丸投げ
	stateMachine_.Update(&ctx_, deltaTime);
}

void PlayerStatusController::HandleMovementInput(const PlayerInputComponent& input, float deltaTime)
{
	const MovementState nextState = input.GetDesiredMovementState();

	if (GetCombatState() != CombatState::None) {
		// Attack Recovery中など、recoveryMoveCancelStart等を過ぎていて
		// 実際に移動入力がある場合のみ、Evade/Attackキャンセルと同じ
		// 考え方で通常移動(None)へキャンセルする。それ以外は従来通り
		// 移動処理を行わない。
		if (!CanStartMove() || nextState == MovementState::Stand) return;
		ChangeStateToNone();
	}

	if (ctx_.movementState != nextState) {
		ctx_.movementState = nextState;
		if (ctx_.combatMovement != nullptr) {
			ctx_.combatMovement->ApplyMovementState(ctx_.movementState);
		}
	}

	if (ctx_.facing != nullptr) {
		const bool shouldFaceMovement = !IsLockedOn() || ctx_.movementState == MovementState::Run;
		ctx_.facing->SetFacingEnabled(shouldFaceMovement);
	}

	if (ctx_.movementAnimation != nullptr) {
		ctx_.movementAnimation->Tick(deltaTime, ctx_.movementState, input.GetMoveDirection(), IsLockedOn());
	}
}

void PlayerStatusController::HandleActionInput(PlayerInputComponent& input)
{
	if (IsStaggered()) return;

	if (input.ConsumeLockPressed()) {
		if (IsLockedOn()) ClearLockOn();
		else TryLockOn();
	}

	if (input.HasCommand(ActionCommand::Evade) && CanStartEvade()) {
		Math::Vector3 direction = Math::Vector3::Zero;
		input.ConsumeCommand(ActionCommand::Evade, direction);
		TryStartEvade(direction);
	}
	else if (input.HasCommand(ActionCommand::Attack) && input.IsGuardHeld()
		&& CanStartAttack() && CanStartGuard()) {
		// 同時押しは個別のAttack/Guardより優先し、両方消費してチャージ開始。
		input.ConsumeCommand(ActionCommand::Attack);
		input.ConsumeCommand(ActionCommand::Guard);
		TryStartCharge();
	}
	else if (input.HasCommand(ActionCommand::Attack) && CanStartAttack()) {
		input.ConsumeCommand(ActionCommand::Attack);
		TryStartAttack();
	}
	else if (input.HasCommand(ActionCommand::Guard) && CanStartGuard()) {
		input.ConsumeCommand(ActionCommand::Guard);
		TryStartGuard();
	}
	else if (!input.IsGuardHeld() && GetCombatState() == CombatState::Guard && CanReleaseGuard()) {
		RequestGuardRelease();
	}
	else if (IsCharging() && !(input.IsAttackHeld() && input.IsGuardHeld())) {
		// どちらかのボタンを離したら発射。
		ReleaseCharge();
	}
}

bool PlayerStatusController::TryStartAttack()
{
	if (!CanStartAttack()) return false;
	if (ctx_.attackSelector == nullptr || !ctx_.attackSelector->TryResolveNext(ActionCommand::Attack)) return false;

	stateAttack_.Setup(1.0f);
	ForceTransitionTo(&stateAttack_);
	return true;
}

bool PlayerStatusController::TryStartEvade(const Math::Vector3& direction)
{
	if (!CanStartEvade()) return false;
	stateEvade_.Setup(direction);
	TransitionTo(&stateEvade_);
	return true;
}

bool PlayerStatusController::TryStartGuard()
{
	if (!CanStartGuard()) return false;
	ForceTransitionTo(&stateGuard_);
	return true;
}

bool PlayerStatusController::TryStartCharge()
{
	if (!CanStartAttack() || !CanStartGuard()) return false;
	ForceTransitionTo(&stateCharge_);
	return true;
}

void PlayerStatusController::ReleaseCharge()
{
	if (!IsCharging()) return;

	// チャージ技が未登録なら、溜めを捨ててNoneへ戻る。
	if (ctx_.attackSelector == nullptr || !ctx_.attackSelector->TryResolveNext(ActionCommand::ChargeAttack)) {
		ChangeStateToNone();
		return;
	}

	ForceTransitionTo(&stateAttack_);
}

void PlayerStatusController::ApplyStagger(bool isLarge)
{
	if (IsDead()) return; // 死亡後のよろけで上書きされない
	stateStagger_.Setup(isLarge); // 継続時間はStateStagger側のdata_から決める
	ForceTransitionTo(&stateStagger_);
}

void PlayerStatusController::Die()
{
	if (IsDead()) return;
	ForceTransitionTo(&stateDead_);
}

void PlayerStatusController::NotifyParrySuccess()
{
	// パリィはGuard中にしか成立し得ないため、Guard中でなければ何もしない
	if (GetCombatState() == CombatState::Guard) {
		stateGuard_.NotifyParrySuccess(&ctx_);
	}
}

void PlayerStatusController::NotifyGuardHit()
{
	if (GetCombatState() == CombatState::Guard) {
		stateGuard_.NotifyGuardHit(&ctx_);
	}
}

void PlayerStatusController::RequestGuardRelease()
{
	if (GetCombatState() == CombatState::Guard) {
		stateGuard_.RequestRelease(&ctx_);
	}
}

void PlayerStatusController::TryLockOn()
{
	if (ctx_.lockOn != nullptr) ctx_.lockOn->TryLockOn();
}

void PlayerStatusController::ClearLockOn()
{
	if (ctx_.lockOn != nullptr) ctx_.lockOn->ClearLockOn();
}

bool PlayerStatusController::IsLockedOn() const
{
	return ctx_.IsLockedOn();
}

void PlayerStatusController::SetWeapon(Handle<WeaponComponent> weapon)
{
	if (WeaponSetComponent* weaponSet = GetOwner()->GetComponent<WeaponSetComponent>()) {
		weaponSet->RegisterWeapon(kMainWeaponSlot, weapon);
	}
}

void PlayerStatusController::TransitionTo(IPlayerState* nextState)
{
	// 通常の(失敗しうる)遷移。prevStateをOnStateChanged側で参照できるよう、
	// 遷移前にCurrent()を取得してから渡す。
	IPlayerState* prevState = stateMachine_.Current();
	if (stateMachine_.TransitionTo(&ctx_, nextState)) {
		OnStateChanged(prevState, nextState);
	}
}

void PlayerStatusController::ForceTransitionTo(IPlayerState* nextState)
{
	// 強制(必ず成功する)遷移。TryStartAttack/ApplyStaggerのように
	// CanStartXxx()を独自に判定済みで、StateMachine側の可否判定を
	// バイパスしたい場合に使う。
	IPlayerState* prevState = stateMachine_.Current();
	stateMachine_.ForceTransitionTo(&ctx_, nextState);
	OnStateChanged(prevState, nextState);
}

void PlayerStatusController::OnStateChanged(IPlayerState* prevState, IPlayerState* nextState)
{
	if (ctx_.combatMovement != nullptr) {
		ctx_.combatMovement->SetMovementEnabled(nextState == &stateNone_);
	}
	if (ctx_.facing != nullptr) {
		// None: movement追従で向く。Attack: FaceAttackTarget()のoverrideで向く。
		// Evade/Guard/Stagger/Deadはfacing完全停止(現在の向きを保つ)。
		ctx_.facing->SetFacingEnabled(nextState == &stateNone_ || nextState == &stateAttack_);

		if (nextState != &stateAttack_) {
			ctx_.facing->ClearAttackFacingOverride();
		}
	}

	if (ctx_.attackSelector == nullptr) return;

	if (nextState == &stateAttack_) {
		// 新しい攻撃を実際に開始した時点で通知する
		// (次にどの技かの選択自体はTryStartAttack内で既に解決済み)。
		ctx_.attackSelector->NotifyAttackStarted();
		return;
	}

	if (prevState == &stateAttack_ && nextState == &stateNone_) {
		// AttackのRecoveryが(中断されずに)自然終了してNoneへ戻った場合のみ、
		// コンボ継続受付ウィンドウを開く。何秒開けるかはAttackData側の
		// 値(cancelData.comboWindowAfterRecovery)を使う。
		ctx_.attackSelector->NotifyRecoveryFinishedNaturally(
			ctx_.attackSelector->GetCurrentAttackData().cancelData.comboWindowAfterRecovery);
		return;
	}

	// Evade/Guard/Staggerへの割り込みなど、それ以外の遷移では
	// コンボ継続を認めず即座に打ち切る。
	ctx_.attackSelector->ResetCombo();
}