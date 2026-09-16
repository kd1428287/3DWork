#include "PlayerStatusController.h"

#include "PlayerAttackSelector.h"
#include "PlayerFacingComponent.h"
#include "PlayerCombatMovementComponent.h"

#include "Application/Definitions/Character/Player/PlayerCombatDataTable.h"
#include "../Combat/HitReactionComponent.h"
#include "../../../Physics/Collision/ColliderComponent.h"

void PlayerStatusController::Awake()
{
	inputComponent_ = GetOwner()->GetComponent<PlayerInputComponent>();
	lockOnComponent_ = GetOwner()->GetComponent<PlayerLockOnComponent>();
	movementAnimationComponent_ = GetOwner()->GetComponent<PlayerMovementAnimationComponent>();
	modelAnimatorComponent_ = GetOwner()->GetComponent<ModelAnimatorComponent>();
	weaponSet_ = GetOwner()->GetComponent<WeaponSetComponent>();

	// 新設した兄弟コンポーネント群。各々が自分の担当データ(コンボ木・
	// Transform・MovementComponent等)を自分のStart()内で解決する。
	attackSelector_ = GetOwner()->GetComponent<PlayerAttackSelector>();
	facing_ = GetOwner()->GetComponent<PlayerFacingComponent>();
	combatMovement_ = GetOwner()->GetComponent<PlayerCombatMovementComponent>();

	// Evade/Guardは分岐を持たない単純な1件データのため、Attackのように
	// 専用コンポーネントへ切り出さず、引き続きここでデバッグ用テーブルから
	// 読み込む。
	baseEvadeData_ = CreateDebugEvadeData();
	baseGuardData_ = CreateDebugGuardData();

	// 被弾時のパリィ/ガード/通常被弾の分岐と、それに伴うダメージ/
	// ノックバック/エフェクト処理はHitReactionComponentへ切り出し済み
	// (HurtBoxへのCollisionEnterEventの購読自体もHitReactionComponent側が
	// 持つため、ここでは購読処理を書かない)。ここでは「今パリィ猶予中か/
	// ガード中か/通常被弾でスタンへ入ってほしい」という問い合わせに
	// 答えられるよう、自分自身をIHitReactionQueryとして登録するだけでよい。
	if (HitReactionComponent* hitReaction = GetOwner()->GetComponent<HitReactionComponent>()) {
		hitReaction->SetQuerySource(this);
		// メイン武器のColliderをHitReactionComponentへ渡す。装備
		// (WeaponSetComponent::RegisterWeapon)がこのStart()より後に
		// 行われる構成の場合はまだ未登録でnullptrになりうる点は
		// 旧実装と同じ制約。
		if (weaponSet_ != nullptr) {
			if (WeaponComponent* mainWeapon = weaponSet_->GetWeapon(kMainWeaponSlot)) {
				if (ColliderComponent* mainWeaponCollider = mainWeapon->GetCollider()) {
					hitReaction->SetWeaponCollider(Handle<ColliderComponent>(mainWeaponCollider));
				}
			}
		}
	}

	// 初期状態のセット。TransitionTo経由なのでEnterも呼ばれる。
	TransitionTo(&stateNone_);
}

void PlayerStatusController::Update(float deltaTime)
{
	if (inputComponent_ != nullptr) {
		HandleMovementInput(*inputComponent_, deltaTime);
		HandleActionInput(*inputComponent_);
	}

	// ロック中の向き固定・移動状態の更新は、いずれもCombatState::Noneの
	// 間だけ行う(Attack/Evade/Guard/Stagger中は各Stateが専有するため)。
	if (GetCombatState() == CombatState::None) {
		if (facing_ != nullptr) {
			facing_->UpdateLockOnFacing(movementState_ == MovementState::Run);
		}
		if (combatMovement_ != nullptr) {
			combatMovement_->UpdateMovementState(movementState_, deltaTime);
		}
	}

	// コンボ継続受付ウィンドウのカウントダウンはattackSelector_自身が持つ
	// (PlayerAttackSelector::Update()参照)。ここでは呼び出さない
	// (Update()はComponentBaseのライフサイクルとしてエンジン側から
	//  自動的に呼ばれる。詳細はPlayerAttackSelector.h参照)。

	// 戦闘状態の更新は共通StateMachineに丸投げ
	stateMachine_.Update(this, deltaTime);
}

void PlayerStatusController::HandleMovementInput(const PlayerInputComponent& input, float deltaTime)
{
	if (GetCombatState() != CombatState::None) return;
	const MovementState nextState = input.GetDesiredMovementState();

	if (movementState_ != nextState) {
		movementState_ = nextState;
		if (combatMovement_ != nullptr) {
			combatMovement_->ApplyMovementState(movementState_);
		}
	}

	if (facing_ != nullptr) {
		const bool shouldFaceMovement = !IsLockedOn() || movementState_ == MovementState::Run;
		facing_->SetFacingEnabled(shouldFaceMovement);
	}

	if (movementAnimationComponent_ != nullptr) {
		movementAnimationComponent_->Tick(deltaTime, movementState_, input.GetMoveDirection(), IsLockedOn());
	}
}

void PlayerStatusController::HandleActionInput(PlayerInputComponent& input)
{
	// スタン中(Stagger)は一切の行動入力を受け付けない。
	if (IsStaggered()) return;

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
	else if (GetCombatState() == CombatState::Guard && CanReleaseGuard()) {
		ChangeStateToNone(); // ガードキーを離したら解除(パリィ成功演出中はCanReleaseGuard()がfalseになり保留される)
	}

	if (input.HasCommand(ActionCommand::Evade) && CanStartEvade()) {
		EvadeData data = baseEvadeData_;
		input.ConsumeCommand(ActionCommand::Evade, data.evadeDirection);
		TryStartEvade(data);
	}
	else if (input.HasCommand(ActionCommand::Attack) && CanStartAttack()) {
		input.ConsumeCommand(ActionCommand::Attack);
		TryStartAttack();
	}
}

EvadeDirection PlayerStatusController::ClassifyEvadeDirection(const Math::Vector3& inputDirection) const
{
	return (facing_ != nullptr) ? facing_->ClassifyEvadeDirection(inputDirection) : EvadeDirection::Forward;
}

bool PlayerStatusController::TryStartAttack()
{
	if (!CanStartAttack()) return false;
	if (attackSelector_ == nullptr || !attackSelector_->TryResolveNext(ActionCommand::Attack)) return false;

	ForceTransitionTo(&stateAttack_);
	return true;
}

bool PlayerStatusController::TryStartEvade(const EvadeData& data)
{
	if (!CanStartEvade()) return false;
	currentEvade_ = data;
	TransitionTo(&stateEvade_);
	return true;
}

bool PlayerStatusController::TryStartGuard()
{
	if (!CanStartGuard()) return false;
	currentGuard_ = baseGuardData_;
	TransitionTo(&stateGuard_);
	return true;
}

void PlayerStatusController::ApplyStagger(bool isLarge, float duration)
{
	stateStagger_.Setup(isLarge, duration);
	ForceTransitionTo(&stateStagger_);
}

void PlayerStatusController::NotifyParrySuccess()
{
	// パリィはGuard中にしか成立し得ないため、Guard中でなければ何もしない
	// (念のためのガード)。
	if (GetCombatState() == CombatState::Guard) {
		stateGuard_.NotifyParrySuccess(this);
	}
}

void PlayerStatusController::NotifyGuardHit()
{
	if (GetCombatState() == CombatState::Guard) {
		stateGuard_.NotifyGuardHit(this);
	}
}

void PlayerStatusController::TryLockOn()
{
	if (lockOnComponent_ != nullptr) lockOnComponent_->TryLockOn();
}

void PlayerStatusController::ClearLockOn()
{
	if (lockOnComponent_ != nullptr) lockOnComponent_->ClearLockOn();
}

bool PlayerStatusController::IsLockedOn() const
{
	return lockOnComponent_ != nullptr && lockOnComponent_->IsLockedOn();
}

void PlayerStatusController::FaceAttackTarget()
{
	if (facing_ != nullptr) facing_->FaceAttackTarget();
}

void PlayerStatusController::SetWeapon(Handle<WeaponComponent> weapon)
{
	if (WeaponSetComponent* weaponSet = GetOwner()->GetComponent<WeaponSetComponent>()) {
		weaponSet->RegisterWeapon(kMainWeaponSlot, weapon);
	}
}

void PlayerStatusController::PlayAnimation(const std::string& name, bool loop, float targetDurationSeconds,
	bool useRootMotion, float blendDurationSeconds)
{
	if (modelAnimatorComponent_ == nullptr) return;
	modelAnimatorComponent_->SetRootMotionBoneName(useRootMotion ? kRootMotionBoneName : "");
	modelAnimatorComponent_->SetBlendDuration(blendDurationSeconds);
	modelAnimatorComponent_->Play(name, loop, targetDurationSeconds);
}

void PlayerStatusController::RefreshMovementAnimation()
{
	if (inputComponent_ == nullptr) return;
	movementState_ = inputComponent_->GetDesiredMovementState();
	if (combatMovement_ != nullptr) {
		combatMovement_->ApplyMovementState(movementState_);
	}

	if (movementAnimationComponent_ != nullptr) {
		movementAnimationComponent_->Refresh(movementState_, inputComponent_->GetMoveDirection(), IsLockedOn());
	}
}

void PlayerStatusController::RequestStepMove(const Math::Vector3& direction, float distance, float duration)
{
	if (combatMovement_ != nullptr) {
		combatMovement_->RequestStepMove(direction, distance, duration);
	}
}

void PlayerStatusController::RequestStepMoveTowardsTarget(const Math::Vector3& fallbackDirection,
	float stepDistance, float engageDistance, float duration)
{
	if (combatMovement_ == nullptr) return;

	GameObject* target = (facing_ != nullptr) ? facing_->GetCurrentAttackTarget() : nullptr;
	combatMovement_->RequestStepMoveTowardsTarget(target, fallbackDirection, stepDistance, engageDistance, duration);
}

void PlayerStatusController::CancelStepMove()
{
	if (combatMovement_ != nullptr) {
		combatMovement_->CancelStepMove();
	}
}

void PlayerStatusController::SetMovementEnabled(bool enabled)
{
	if (combatMovement_ != nullptr) {
		combatMovement_->SetMovementEnabled(enabled);
	}
}

void PlayerStatusController::TransitionTo(IPlayerState* nextState)
{
	// 通常の(失敗しうる)遷移。prevStateをOnStateChanged側で参照できるよう、
	// 遷移前にCurrent()を取得してから渡す。
	IPlayerState* prevState = stateMachine_.Current();
	if (stateMachine_.TransitionTo(this, nextState)) {
		OnStateChanged(prevState, nextState);
	}
}

void PlayerStatusController::ForceTransitionTo(IPlayerState* nextState)
{
	// 強制(必ず成功する)遷移。TryStartAttack/ApplyStaggerのように
	// CanStartXxx()を独自に判定済みで、StateMachine側の可否判定を
	// バイパスしたい場合に使う。
	IPlayerState* prevState = stateMachine_.Current();
	stateMachine_.ForceTransitionTo(this, nextState);
	OnStateChanged(prevState, nextState);
}

void PlayerStatusController::OnStateChanged(IPlayerState* prevState, IPlayerState* nextState)
{
	if (combatMovement_ != nullptr) {
		combatMovement_->SetMovementEnabled(nextState == &stateNone_);
	}
	if (facing_ != nullptr) {
		facing_->SetFacingEnabled(nextState == &stateNone_);
	}

	if (attackSelector_ == nullptr) return;

	if (nextState == &stateAttack_) {
		// 新しい攻撃を実際に開始した時点で通知する
		// (次にどの技かの選択自体はTryStartAttack内で既に解決済み)。
		attackSelector_->NotifyAttackStarted();
		return;
	}

	if (prevState == &stateAttack_ && nextState == &stateNone_) {
		// AttackのRecoveryが(中断されずに)自然終了してNoneへ戻った場合のみ、
		// コンボ継続受付ウィンドウを開く。何秒開けるかはAttackData側の
		// 値(cancelData.comboWindowAfterRecovery)を使う。
		attackSelector_->NotifyRecoveryFinishedNaturally(
			attackSelector_->GetCurrentAttackData().cancelData.comboWindowAfterRecovery);
		return;
	}

	// Evade/Guard/Staggerへの割り込みなど、それ以外の遷移では
	// コンボ継続を認めず即座に打ち切る。
	attackSelector_->ResetCombo();
}