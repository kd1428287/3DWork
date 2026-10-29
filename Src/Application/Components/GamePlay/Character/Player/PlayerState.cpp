// PlayerState.cpp (StateAttackのCanStartXxx群のみ抜粋変更・他は元のまま)
#include "PlayerState.h"
#include "PlayerStateContext.h"
#include "PlayerAttackSelector.h"
#include "PlayerInputComponent.h"
#include "PlayerLockOnComponent.h"
#include "PlayerMovementAnimationComponent.h"
#include "PlayerFacingComponent.h"
#include "PlayerCombatMovementComponent.h"
#include "../Common/WeaponSetComponent.h"
#include "../../../Physics/Movement/TweenMoveComponent.h"
#include "Application/Core/EventBus/Events/SceneEvents.h" // パスは配置に合わせて調整

// =================================================================
// 各Stateの具体的なロジック実装
// =================================================================

// --- None State ---
void StateNone::Enter(PlayerStateContext* ctx) {
	// 戦闘行動からNoneへ復帰した直後、現在の入力に合わせてIdle/Walk/Runを同期し直す。
	if (ctx->input == nullptr) return;
	ctx->movementState = ctx->input->GetDesiredMovementState();
	ctx->combatMovement->ApplyMovementState(ctx->movementState);
	if (ctx->movementAnimation != nullptr) {
		ctx->movementAnimation->Refresh(ctx->movementState, ctx->input->GetMoveDirection(), ctx->IsLockedOn());
	}
}

// --- Attack State ---
void StateAttack::Enter(PlayerStateContext* ctx) {
	phase_ = CombatState::AttackWindup;
	elapsed_ = 0.0f;


	// ロック中ならロック対象へ、未ロックなら画面中心に最も近い敵へ正対する。
	ctx->facing->FaceAttackTarget();
	//ctx->combatMovement->SetMovementEnabled(false);

	const AttackData& data = ctx->attackSelector->GetCurrentAttackData();
	auto damageData = data.damageData;
	damageData.damage *= damageScale_; // チャージ倍率(通常攻撃は1.0)
	ctx->weaponSet->SetAttackDamageData(data.weaponSlots, damageData);
	// 演出データ(SE/エフェクト等)も対象スロットの武器へ渡す。
	for (const std::string& slot : data.weaponSlots) {
		if (WeaponComponent* weapon = ctx->weaponSet->GetWeapon(slot)) weapon->SetAttackVisualData(data.visualData);
	}

	ctx->PlayAnimation(data.phaseData.windup);
}

void StateAttack::Update(PlayerStateContext* ctx, float deltaTime) {
	elapsed_ += deltaTime;
	const AttackData& data = ctx->attackSelector->GetCurrentAttackData();

	if (phase_ == CombatState::AttackWindup && elapsed_ >= data.phaseData.windup.duration) {
		phase_ = CombatState::AttackActive;
		elapsed_ = 0.0f;

		if (data.phaseData.active.useRootMotion) {
			ctx->RequestRootMotionWarpTowardsTarget(data.moveData.stepDistance, data.moveData.engageDistance,
				kMinRootMotionWarpScale, kMaxRootMotionWarpScale);
		}
		else {
			ctx->RequestStepMoveTowardsTarget(data.moveData.stepDirection, data.moveData.stepDistance,
				data.moveData.engageDistance, data.moveData.stepDuration);
		}
		ctx->weaponSet->SetHitBoxEnabled(data.weaponSlots, true); // 攻撃判定が実際に発生する一瞬だけ有効化
		ctx->weaponSet->SetTrailEmitting(data.weaponSlots, true); // 武器の軌跡エフェクトもHitBoxと同じ窓で記録開始

		ctx->PlayAnimation(data.phaseData.active);
		ctx->PlaySE(data_.swingSE); // 振りの音は判定発生に合わせる
	}
	else if (phase_ == CombatState::AttackActive && elapsed_ >= data.phaseData.active.duration) {
		phase_ = CombatState::AttackRecovery;
		elapsed_ = 0.0f;
		ctx->combatMovement->ClearRootMotionWarp(); // 踏み込み区間の終了(Tween側はCancelStepMoveが別途処理)
		ctx->weaponSet->SetHitBoxEnabled(data.weaponSlots, false); // 判定の発生窓を閉じる
		ctx->weaponSet->SetTrailEmitting(data.weaponSlots, false); // 軌跡エフェクトの記録も停止(既に生成済みの頂点はStopEmit後も自然に流れて消える)

		ctx->PlayAnimation(data.phaseData.recovery);
	}
	else if (phase_ == CombatState::AttackRecovery && elapsed_ >= data.phaseData.recovery.duration) {
		// 自律的に終了し、ControllerにNoneへの復帰を要請する
		ctx->requester->ChangeStateToNone();
	}
}

void StateAttack::Exit(PlayerStateContext* ctx) {
	// 強制中断された場合のガード
	ctx->combatMovement->CancelStepMove();
	ctx->combatMovement->ClearRootMotionWarp();
	ctx->combatMovement->SetMovementEnabled(true);

	const AttackData& data = ctx->attackSelector->GetCurrentAttackData();
	ctx->weaponSet->SetHitBoxEnabled(data.weaponSlots, false);
	ctx->weaponSet->SetTrailEmitting(data.weaponSlots, false);
}

bool StateAttack::CanStartEvade(const PlayerStateContext* ctx) const {
	if (phase_ == CombatState::AttackRecovery) {
		return elapsed_ >= ctx->attackSelector->GetCurrentAttackData().cancelData.recoveryEvadeCancelStart;
	}
	return false;
}

bool StateAttack::CanStartAttack(const PlayerStateContext* ctx) const {
	// Recovery中の一定タイミングを過ぎたら、次の攻撃(コンボ)へのキャンセルを許可する
	if (phase_ == CombatState::AttackRecovery) {
		return elapsed_ >= ctx->attackSelector->GetCurrentAttackData().cancelData.recoveryAttackCancelStart;
	}
	return false;
}

bool StateAttack::CanStartGuard(const PlayerStateContext* ctx) const {
	if (phase_ == CombatState::AttackRecovery) {
		//return elapsed_ >= ctx->attackSelector->GetCurrentAttackData().cancelData.recoveryEvadeCancelStart;
		return true;
	}
	return false;
}

bool StateAttack::CanStartMove(const PlayerStateContext* ctx) const {
	// Recovery中の一定タイミングを過ぎたら、移動入力による通常移動への
	// キャンセルを許可する。CanStartEvade/CanStartAttackと同じ考え方。
	if (phase_ == CombatState::AttackRecovery) {
		return elapsed_ >= ctx->attackSelector->GetCurrentAttackData().cancelData.recoveryMoveCancelStart;
	}
	return false;
}


// --- Evade State ---
void StateEvade::Enter(PlayerStateContext* ctx) {
	phase_ = CombatState::Evade;
	elapsed_ = 0.0f;

	// 回避中の移動は入力ではなく、決め打ちの軌道(RequestStepMove)、
	// または(useRootMotionがtrueの場合)アニメーションのルートモーションに
	// 任せる。MovementComponentはTransitionTo側で既に無効化されているため、
	// 位置を書き換える権利がここで競合することはない。
	const auto& data = data_;

	// 現在の前方に対する入力方向の相対位置(前後左右)を判定し、
	// 対応するアニメーションを選ぶ。キャラクター自体は向きを変えない
	// (facingDirectionComponent_はEvade中無効化されているため、
	//  ここで回転させない限り自然に維持される)。
	const EvadeDirection evadeDir = ctx->facing->ClassifyEvadeDirection(direction_);

	// 回避全体(Active+Recovery)の秒数を目標としてアニメーション速度を
	// 自動スケーリングする(詳細はModelAnimatorComponent::Play参照)。
	// 【未対応】EvadeはAttackと異なり、まだ「1回避=1クリップ」のまま
	// フェーズ分割していない(前後左右4方向とのかけ合わせ方を先に
	// 決める必要があるため。詳細は別途相談)。
	const float targetDuration = data.activeDuration + data.recoveryDuration;
	ctx->PlayAnimation(data.GetAnimationName(evadeDir), false, targetDuration, data.useRootMotion);
	ctx->PlaySE(data.se);
	if (!data.useRootMotion) {
		//ctx->RequestStepMove(direction_, data.evadeDistance, data.activeDuration + data.recoveryDuration);
	}
}

void StateEvade::Exit(PlayerStateContext* ctx) {
	ctx->combatMovement->CancelStepMove();
}

void StateEvade::Update(PlayerStateContext* ctx, float deltaTime) {
	elapsed_ += deltaTime;
	const auto& data = data_;
	if (phase_ == CombatState::Evade && elapsed_ >= data.activeDuration) {
		phase_ = CombatState::EvadeRecovery;
		elapsed_ = 0.0f;

	}
	else if (phase_ == CombatState::EvadeRecovery && elapsed_ >= data.recoveryDuration) {
		ctx->requester->ChangeStateToNone();
	}
}

bool StateEvade::IsInJustEvadeWindow(const PlayerStateContext* ctx) const {
	if (phase_ != CombatState::Evade) return false;
	const auto& data = data_;
	return elapsed_ >= data.justWindowStart && elapsed_ <= data.justWindowEnd;
}

bool StateEvade::IsInvincible(const PlayerStateContext* ctx) const {
	// Evade(実移動フェーズ)中のみ無敵。EvadeRecoveryは無敵切れとして
	// 通常の被弾判定に戻す(後隙に攻撃を合わせられたら普通に食らう)。
	return phase_ == CombatState::Evade;
}


// --- Guard State ---
void StateGuard::Enter(PlayerStateContext* ctx) {
	elapsed_ = 0.0f;
	hasEnteredLoop_ = false;
	parrySucceeded_ = false;
	parrySuccessElapsed_ = 0.0f;
	isReactingToGuardHit_ = false;
	guardHitElapsed_ = 0.0f;
	isReleasing_ = false;
	releaseElapsed_ = 0.0f;

	// Guard中のルートモーション移動量を一律に抑制する(動きすぎ対策)。
	ctx->combatMovement->SetRootMotionScale(data_.rootMotionScale);

	// 構え動作を単発再生する
	ctx->PlayAnimation(data_.start);
	ctx->PlaySE(data_.raiseSE);
}

void StateGuard::Exit(PlayerStateContext* ctx) {
	ctx->combatMovement->ClearRootMotionWarp();
}

void StateGuard::Update(PlayerStateContext* ctx, float deltaTime) {
	elapsed_ += deltaTime;

	// 解除要求後は終了アニメーションの再生完了を待つだけの状態。
	// endDuration経過した時点で、ここで初めて自律的にNoneへ戻る
	// (StateAttackのRecovery終了/StateEvadeのEvadeRecovery終了と同じ考え方)。
	if (isReleasing_) {
		releaseElapsed_ += deltaTime;
		if (releaseElapsed_ >= data_.end.duration) {
			ctx->requester->ChangeStateToNone();
		}
		return;
	}

	// Guardは継続状態なので、時間経過による自動終了はない

	// 構え動作(単発)が終わったら、継続姿勢のLoopへ切り替える。
	if (!hasEnteredLoop_ && elapsed_ >= data_.start.duration) {
		hasEnteredLoop_ = true;
		ctx->PlayAnimation(data_.loop);
	}

	if (parrySucceeded_) {
		parrySuccessElapsed_ += deltaTime;
		if (parrySuccessElapsed_ >= data_.parry.duration) {
			// 演出終了。NormalBlockへ復帰する(ガードキーが既に離されていれば
			// 次フレームのCanReleaseGuard()判定でHandleActionInput側が解除する)。
			parrySucceeded_ = false;
			ResumeLoopAnimation(ctx);
		}
	}
	else if (isReactingToGuardHit_) {
		guardHitElapsed_ += deltaTime;
		if (guardHitElapsed_ >= data_.hit.duration) {
			isReactingToGuardHit_ = false;
			ResumeLoopAnimation(ctx);
		}
	}
}

bool StateGuard::IsInParryWindow(const PlayerStateContext* ctx) const {
	return GetGuardPhase(ctx) == GuardPhase::JustWindow;
}

bool StateGuard::CanReleaseGuard(const PlayerStateContext* ctx) const {
	// パリィ成功演出中だけ解除を保留し、演出を強制的に見せ切る。
	return !parrySucceeded_;
}

void StateGuard::RequestRelease(PlayerStateContext* ctx) {
	if (isReleasing_) return; // 多重要求防止(HandleActionInputは毎フレーム呼んでくる)

	isReleasing_ = true;
	releaseElapsed_ = 0.0f;

	parrySucceeded_ = false;
	isReactingToGuardHit_ = false;
	hasEnteredLoop_ = true;

	ctx->PlayAnimation(data_.end);
}

bool StateGuard::CanStartAttack(const PlayerStateContext* ctx) const
{
	return true;
}

bool StateGuard::CanStartEvade(const PlayerStateContext* ctx) const
{
	return isReleasing_;
}

bool StateGuard::CanStartGuard(const PlayerStateContext* ctx) const
{
	return true;
}

bool StateGuard::CanStartMove(const PlayerStateContext* ctx) const
{
	return isReleasing_;
}

void StateGuard::NotifyParrySuccess(PlayerStateContext* ctx)
{
	if (parrySucceeded_) return; // 同一パリィ猶予内での多重成立を防止
	parrySucceeded_ = true;
	parrySuccessElapsed_ = 0.0f;
	isReactingToGuardHit_ = false; // ガードヒット演出より優先して上書きする
	hasEnteredLoop_ = true;

	ctx->PlayAnimation(data_.parry);
}

void StateGuard::NotifyGuardHit(PlayerStateContext* ctx) 
{
	// パリィ成功演出中はそちらを優先し、上書きしない。
	if (parrySucceeded_) return;

	// 単発リアクションを都度再生し直す
	isReactingToGuardHit_ = true;
	guardHitElapsed_ = 0.0f;
	hasEnteredLoop_ = true;

	ctx->PlayAnimation(data_.hit);
}

void StateGuard::ResumeLoopAnimation(PlayerStateContext* ctx) 
{
	hasEnteredLoop_ = true;
	ctx->PlayAnimation(data_.loop);
}

StateGuard::GuardPhase StateGuard::GetGuardPhase(const PlayerStateContext* ctx) const 
{
	if (isReleasing_) return GuardPhase::Release;
	if (parrySucceeded_) return GuardPhase::ParrySuccess;
	return elapsed_ <= data_.justWindowDuration
		? GuardPhase::JustWindow
		: GuardPhase::NormalBlock;
}


// --- Charge State ---
void StateCharge::Enter(PlayerStateContext* ctx) 
{
	elapsed_ = 0.0f;


	// ロック中ならロック対象へ、未ロックなら画面中心に最も近い敵へ正対する。
	ctx->facing->FaceAttackTarget();
	
	const ChargeData& data = GetData();
	ctx->PlayAnimation(data.charge);
}

void StateCharge::Update(PlayerStateContext* ctx, float deltaTime) {
	// 最大値のクランプはChargeData::GetDamageScale側で行う(最大到達後は保持)。
	elapsed_ += deltaTime;
}

void StateCharge::Exit(PlayerStateContext* ctx) 
{
	const ChargeData& data = GetData();
	if (data.GetChargeScale(elapsed_) >= 1.0f) {

	}
	else {

	}
}


// --- Stagger State ---
void StateStagger::Enter(PlayerStateContext* ctx) {
	phase_ = Phase::Reaction;
	elapsed_ = 0.0f;

	const StaggerPhaseData& data = GetPhaseData(ctx);
	reactionDuration_ = data.reactionDuration; // 秒数はStaggerData側で決める

	ctx->combatMovement->SetRootMotionScale(data.rootMotionScale);
	ctx->PlayAnimation(data.animationName, false, reactionDuration_,
		static_cast<float>(data.reactionStartFrame), static_cast<float>(data.reactionEndFrame),
		data.useRootMotion, data.reactionBlendDuration);
}

void StateStagger::Update(PlayerStateContext* ctx, float deltaTime) {
	elapsed_ += deltaTime;
	const StaggerPhaseData& data = GetPhaseData(ctx);

	if (phase_ == Phase::Reaction && elapsed_ >= reactionDuration_) {
		phase_ = Phase::Recovery;
		elapsed_ = 0.0f;
		ctx->PlayAnimation(data.animationName, false, data.recoveryDuration,
			static_cast<float>(data.reactionEndFrame), static_cast<float>(data.recoveryEndFrame),
			data.useRootMotion, data.recoveryBlendDuration);
	}
	else if (phase_ == Phase::Recovery && elapsed_ >= data.recoveryDuration) {
		ctx->requester->ChangeStateToNone();
	}
}

void StateStagger::Exit(PlayerStateContext* ctx)
{
	ctx->combatMovement->ClearRootMotionWarp();
}

bool StateStagger::CanStartEvade(const PlayerStateContext* ctx) const {
	if (phase_ != Phase::Recovery) return false;
	return elapsed_ >= GetPhaseData(ctx).recoveryEvadeCancelStart;
}

bool StateStagger::CanStartMove(const PlayerStateContext* ctx) const {
	return CanStartEvade(ctx);
}

const StaggerPhaseData& StateStagger::GetPhaseData(const PlayerStateContext* ctx) const {
	return isLarge_ ? data_.staggerLarge : data_.staggerSmall;
}


// --- Dead State ---
void StateDead::Enter(PlayerStateContext* ctx) {
	elapsed_ = 0.0f;
	reloadRequested_ = false;
	ctx->combatMovement->CancelStepMove();
	ctx->combatMovement->ClearRootMotionWarp();
	ctx->lockOn->ClearLockOn(); // カメラが死体を追い続けないようロック解除
	ctx->PlayAnimation(data_.animationName, false, data_.animationDuration, false);
	ctx->PlaySE(data_.se);
}

void StateDead::Update(PlayerStateContext* ctx, float deltaTime) {
	elapsed_ += deltaTime;
	if (reloadRequested_ || elapsed_ < data_.animationDuration) return;

	// アニメ終了時に一度だけシーンのリロードを要求する。
	reloadRequested_ = true;
	//GLOBALEVENT->Publish(Events::ReloadingScene{});
	GLOBALEVENT->Publish(Events::SceneChangeRequest{ SceneType::Title });
}

void StateDead::Exit(PlayerStateContext* ctx) {
	// 復活を実装する際に使う。現状は何もしない。
}