// PlayerState.cpp (StateAttackのCanStartXxx群のみ抜粋変更・他は元のまま)
#include "PlayerStatusController.h"
#include "PlayerAttackSelector.h"
#include "../Common/WeaponSetComponent.h"
#include "../../../Physics/Movement/TweenMoveComponent.h"

// =================================================================
// 各Stateの具体的なロジック実装
// =================================================================

// --- None State ---
void StateNone::Enter(PlayerStatusController* controller) {
	controller->RefreshMovementAnimation();
}

// --- Attack State ---
void StateAttack::Enter(PlayerStatusController* controller) {
	phase_ = CombatState::AttackWindup;
	elapsed_ = 0.0f;

	weaponSet_ = controller->GetOwner()->GetComponent<WeaponSetComponent>();
	attackSelector_ = controller->GetOwner()->GetComponent<PlayerAttackSelector>();

	// ロック中ならロック対象へ、未ロックなら画面中心に最も近い敵へ正対する。
	controller->FaceAttackTarget();
	//controller->SetMovementEnabled(false);

	const AttackData& data = attackSelector_->GetCurrentAttackData();
	auto damageData = data.damageData;
	damageData.damage *= controller->GetAttackDamageScale(); // チャージ倍率(通常攻撃は1.0)
	weaponSet_->SetAttackDamageData(data.weaponSlots, damageData);

	controller->PlayAnimation(data.phaseData.windup);
}

void StateAttack::Update(PlayerStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;
	const AttackData& data = attackSelector_->GetCurrentAttackData();

	if (phase_ == CombatState::AttackWindup && elapsed_ >= data.phaseData.windup.duration) {
		phase_ = CombatState::AttackActive;
		elapsed_ = 0.0f;
		controller->RequestStepMoveTowardsTarget(data.moveData.stepDirection, data.moveData.stepDistance,
			data.moveData.engageDistance, data.moveData.stepDuration);
		weaponSet_->SetHitBoxEnabled(data.weaponSlots, true); // 攻撃判定が実際に発生する一瞬だけ有効化
		weaponSet_->SetTrailEmitting(data.weaponSlots, true); // 武器の軌跡エフェクトもHitBoxと同じ窓で記録開始

		controller->PlayAnimation(data.phaseData.active);
	}
	else if (phase_ == CombatState::AttackActive && elapsed_ >= data.phaseData.active.duration) {
		phase_ = CombatState::AttackRecovery;
		elapsed_ = 0.0f;
		weaponSet_->SetHitBoxEnabled(data.weaponSlots, false); // 判定の発生窓を閉じる
		weaponSet_->SetTrailEmitting(data.weaponSlots, false); // 軌跡エフェクトの記録も停止(既に生成済みの頂点はStopEmit後も自然に流れて消える)

		controller->PlayAnimation(data.phaseData.recovery);
	}
	else if (phase_ == CombatState::AttackRecovery && elapsed_ >= data.phaseData.recovery.duration) {
		// 自律的に終了し、ControllerにNoneへの復帰を要請する
		controller->ChangeStateToNone();
	}
}

void StateAttack::Exit(PlayerStatusController* controller) {
	// 強制中断された場合のガード
	controller->CancelStepMove();
	controller->SetMovementEnabled(true);

	const AttackData& data = attackSelector_->GetCurrentAttackData();
	weaponSet_->SetHitBoxEnabled(data.weaponSlots, false);
	weaponSet_->SetTrailEmitting(data.weaponSlots, false);
}

bool StateAttack::CanStartEvade(const PlayerStatusController* controller) const {
	if (phase_ == CombatState::AttackRecovery) {
		return elapsed_ >= attackSelector_->GetCurrentAttackData().cancelData.recoveryEvadeCancelStart;
	}
	return false;
}

bool StateAttack::CanStartAttack(const PlayerStatusController* controller) const {
	// Recovery中の一定タイミングを過ぎたら、次の攻撃(コンボ)へのキャンセルを許可する
	if (phase_ == CombatState::AttackRecovery) {
		return elapsed_ >= attackSelector_->GetCurrentAttackData().cancelData.recoveryAttackCancelStart;
	}
	return false;
}

bool StateAttack::CanStartGuard(const PlayerStatusController* controller) const {
	if (phase_ == CombatState::AttackRecovery) {
		//return elapsed_ >= attackSelector_->GetCurrentAttackData().cancelData.recoveryEvadeCancelStart;
		return true;
	}
	return false;
}

bool StateAttack::CanStartMove(const PlayerStatusController* controller) const {
	// Recovery中の一定タイミングを過ぎたら、移動入力による通常移動への
	// キャンセルを許可する。CanStartEvade/CanStartAttackと同じ考え方。
	if (phase_ == CombatState::AttackRecovery) {
		return elapsed_ >= attackSelector_->GetCurrentAttackData().cancelData.recoveryMoveCancelStart;
	}
	return false;
}


// --- Evade State ---
void StateEvade::Enter(PlayerStatusController* controller) {
	phase_ = CombatState::Evade;
	elapsed_ = 0.0f;

	// 回避中の移動は入力ではなく、決め打ちの軌道(RequestStepMove)、
	// または(useRootMotionがtrueの場合)アニメーションのルートモーションに
	// 任せる。MovementComponentはTransitionTo側で既に無効化されているため、
	// 位置を書き換える権利がここで競合することはない。
	const auto& data = controller->GetCurrentEvadeData();

	// 現在の前方に対する入力方向の相対位置(前後左右)を判定し、
	// 対応するアニメーションを選ぶ。キャラクター自体は向きを変えない
	// (facingDirectionComponent_はEvade中無効化されているため、
	//  ここで回転させない限り自然に維持される)。
	const EvadeDirection evadeDir = controller->ClassifyEvadeDirection(data.evadeDirection);

	// 回避全体(Active+Recovery)の秒数を目標としてアニメーション速度を
	// 自動スケーリングする(詳細はModelAnimatorComponent::Play参照)。
	// 【未対応】EvadeはAttackと異なり、まだ「1回避=1クリップ」のまま
	// フェーズ分割していない(前後左右4方向とのかけ合わせ方を先に
	// 決める必要があるため。詳細は別途相談)。
	const float targetDuration = data.activeDuration + data.recoveryDuration;
	controller->PlayAnimation(data.GetAnimationName(evadeDir), false, targetDuration, data.useRootMotion);
	if (!data.useRootMotion) {
		//controller->RequestStepMove(data.evadeDirection, data.evadeDistance, data.activeDuration + data.recoveryDuration);
	}
}

void StateEvade::Exit(PlayerStatusController* controller) {
	controller->CancelStepMove();
}

void StateEvade::Update(PlayerStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;
	const auto& data = controller->GetCurrentEvadeData();
	if (phase_ == CombatState::Evade && elapsed_ >= data.activeDuration) {
		phase_ = CombatState::EvadeRecovery;
		elapsed_ = 0.0f;

	}
	else if (phase_ == CombatState::EvadeRecovery && elapsed_ >= data.recoveryDuration) {
		controller->ChangeStateToNone();
	}
}

bool StateEvade::IsInJustEvadeWindow(const PlayerStatusController* controller) const {
	if (phase_ != CombatState::Evade) return false;
	const auto& data = controller->GetCurrentEvadeData();
	return elapsed_ >= data.justWindowStart && elapsed_ <= data.justWindowEnd;
}

bool StateEvade::IsInvincible(const PlayerStatusController* controller) const {
	// Evade(実移動フェーズ)中のみ無敵。EvadeRecoveryは無敵切れとして
	// 通常の被弾判定に戻す(後隙に攻撃を合わせられたら普通に食らう)。
	return phase_ == CombatState::Evade;
}


// --- Guard State ---
void StateGuard::Enter(PlayerStatusController* controller) {
	elapsed_ = 0.0f;
	hasEnteredLoop_ = false;
	parrySucceeded_ = false;
	parrySuccessElapsed_ = 0.0f;
	isReactingToGuardHit_ = false;
	guardHitElapsed_ = 0.0f;
	isReleasing_ = false;
	releaseElapsed_ = 0.0f;

	// 構え動作を単発再生する
	controller->PlayAnimation(controller->GetCurrentGuardData().start);
}

void StateGuard::Update(PlayerStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;

	// 解除要求後は終了アニメーションの再生完了を待つだけの状態。
	// endDuration経過した時点で、ここで初めて自律的にNoneへ戻る
	// (StateAttackのRecovery終了/StateEvadeのEvadeRecovery終了と同じ考え方)。
	if (isReleasing_) {
		releaseElapsed_ += deltaTime;
		if (releaseElapsed_ >= controller->GetCurrentGuardData().end.duration) {
			controller->ChangeStateToNone();
		}
		return;
	}

	// Guardは継続状態なので、時間経過による自動終了はない

	// 構え動作(単発)が終わったら、継続姿勢のLoopへ切り替える。
	if (!hasEnteredLoop_ && elapsed_ >= controller->GetCurrentGuardData().start.duration) {
		hasEnteredLoop_ = true;
		controller->PlayAnimation(controller->GetCurrentGuardData().loop);
	}

	if (parrySucceeded_) {
		parrySuccessElapsed_ += deltaTime;
		if (parrySuccessElapsed_ >= controller->GetCurrentGuardData().parry.duration) {
			// 演出終了。NormalBlockへ復帰する(ガードキーが既に離されていれば
			// 次フレームのCanReleaseGuard()判定でHandleActionInput側が解除する)。
			parrySucceeded_ = false;
			ResumeLoopAnimation(controller);
		}
	}
	else if (isReactingToGuardHit_) {
		guardHitElapsed_ += deltaTime;
		if (guardHitElapsed_ >= controller->GetCurrentGuardData().hit.duration) {
			isReactingToGuardHit_ = false;
			ResumeLoopAnimation(controller);
		}
	}
}

bool StateGuard::IsInParryWindow(const PlayerStatusController* controller) const {
	return GetGuardPhase(controller) == GuardPhase::JustWindow;
}

bool StateGuard::CanReleaseGuard(const PlayerStatusController* controller) const {
	// パリィ成功演出中だけ解除を保留し、演出を強制的に見せ切る。
	return !parrySucceeded_;
}

void StateGuard::RequestRelease(PlayerStatusController* controller) {
	if (isReleasing_) return; // 多重要求防止(HandleActionInputは毎フレーム呼んでくる)

	isReleasing_ = true;
	releaseElapsed_ = 0.0f;

	parrySucceeded_ = false;
	isReactingToGuardHit_ = false;
	hasEnteredLoop_ = true;

	controller->PlayAnimation(controller->GetCurrentGuardData().end);
}

bool StateGuard::CanStartAttack(const PlayerStatusController* controller) const
{
	return true;
}

bool StateGuard::CanStartEvade(const PlayerStatusController* controller) const
{
	return isReleasing_;
}

bool StateGuard::CanStartGuard(const PlayerStatusController* controller) const
{
	return true;
}

bool StateGuard::CanStartMove(const PlayerStatusController* controller) const
{
	return isReleasing_;
}

void StateGuard::NotifyParrySuccess(PlayerStatusController* controller) {
	if (parrySucceeded_) return; // 同一パリィ猶予内での多重成立を防止
	parrySucceeded_ = true;
	parrySuccessElapsed_ = 0.0f;
	isReactingToGuardHit_ = false; // ガードヒット演出より優先して上書きする
	hasEnteredLoop_ = true;

	controller->PlayAnimation(controller->GetCurrentGuardData().parry);
}

void StateGuard::NotifyGuardHit(PlayerStatusController* controller) {
	// パリィ成功演出中はそちらを優先し、上書きしない。
	if (parrySucceeded_) return;

	// 単発リアクションを都度再生し直す
	isReactingToGuardHit_ = true;
	guardHitElapsed_ = 0.0f;
	hasEnteredLoop_ = true;

	controller->PlayAnimation(controller->GetCurrentGuardData().hit);
}

void StateGuard::ResumeLoopAnimation(PlayerStatusController* controller) {
	hasEnteredLoop_ = true;
	controller->PlayAnimation(controller->GetCurrentGuardData().loop);
}

StateGuard::GuardPhase StateGuard::GetGuardPhase(const PlayerStatusController* controller) const {
	if (isReleasing_) return GuardPhase::Release;
	if (parrySucceeded_) return GuardPhase::ParrySuccess;
	return elapsed_ <= controller->GetCurrentGuardData().justWindowDuration
		? GuardPhase::JustWindow
		: GuardPhase::NormalBlock;
}


// --- Charge State ---
void StateCharge::Enter(PlayerStatusController* controller) {
	elapsed_ = 0.0f;
	controller->PlayAnimation(controller->GetChargeData().loop);
}

void StateCharge::Update(PlayerStatusController* controller, float deltaTime) {
	// 最大値のクランプはChargeData::GetDamageScale側で行う(最大到達後は保持)。
	elapsed_ += deltaTime;
}


// --- Stagger State ---
void StateStagger::Enter(PlayerStatusController* controller) {
	elapsed_ = 0.0f;
	KdDebugGUI::Instance().AddLog("Stagger");

	// アニメーション未実装のためコメントアウト。
	// AttackData/GuardDataのような専用データ構造をStaggerは
	// 持たないため、isLarge_で仮のアニメーション名を直接出し分ける想定だった。
	controller->PlayAnimation(isLarge_ ? "Large_Hit_Root" : "Hit_B_Root");
}

void StateStagger::Update(PlayerStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;
	if (elapsed_ >= duration_) {
		controller->ChangeStateToNone();
	}
}

void StateStagger::Exit(PlayerStatusController* controller)
{}