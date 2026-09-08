#include "PlayerStatusController.h"
#include "../../Movement/TweenMoveComponent.h"

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

	// ロック中ならロック対象へ、未ロックなら画面中心に最も近い敵へ正対する。
	// facingDirectionComponent_はAttack中無効化されているため、
	// ここで明示的に向きを合わせておく必要がある。
	controller->FaceAttackTarget();

	// 具体的なコンポーネント操作(Transform/TweenMoveComponent/
	// ModelAnimatorComponent)はController側に閉じ込め、Stateは
	// それを直接知らなくてよいようにする。
	const auto& data = controller->GetCurrentAttackData();
	// 攻撃全体(Windup+Active+Recovery)の秒数を目標としてアニメーション
	// 速度を自動スケーリングする(詳細はModelAnimatorComponent::Play参照)。
	const float targetDuration = data.windupDuration + data.activeDuration + data.recoveryDuration;
	controller->PlayAnimation(data.animationName, false, targetDuration, data.useRootMotion, data.blendDuration); // コンボ段数に応じたアニメーション

	if (!data.useRootMotion) {
		controller->RequestStepMoveTowardsTarget(data.stepDirection, data.stepDistance, data.engageDistance, data.stepDuration);
	}
}

void StateAttack::Update(PlayerStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;
	const auto& data = controller->GetCurrentAttackData();

	if (phase_ == CombatState::AttackWindup && elapsed_ >= data.windupDuration) {
		phase_ = CombatState::AttackActive;
		elapsed_ = 0.0f;
	
		controller->SetWeaponHitBoxEnabled(data.weaponSlots, true); // 攻撃判定が実際に発生する一瞬だけ有効化
		controller->SetWeaponTrailEmitting(data.weaponSlots, true); // 武器の軌跡エフェクトもHitBoxと同じ窓で記録開始
	}
	else if (phase_ == CombatState::AttackActive && elapsed_ >= data.activeDuration) {
		phase_ = CombatState::AttackRecovery;
		elapsed_ = 0.0f;
		controller->SetWeaponHitBoxEnabled(data.weaponSlots, false); // 判定の発生窓を閉じる
		controller->SetWeaponTrailEmitting(data.weaponSlots, false); // 軌跡エフェクトの記録も停止(既に生成済みの頂点はStopEmit後も自然に流れて消える)
	}
	else if (phase_ == CombatState::AttackRecovery && elapsed_ >= data.recoveryDuration) {
		controller->ChangeStateToNone();
	}
}

void StateAttack::Exit(PlayerStatusController* controller) {
	// Windup中にStagger等で強制的に割り込まれた場合など、通常のUpdateの
	// 遷移では回収できないタイミングでもステップ移動が残らないよう、
	// Exitで必ず後始末する(Evadeと同じ考え方)。
	controller->CancelStepMove();

	const auto& data = controller->GetCurrentAttackData();

	// AttackActive中に割り込まれた場合、HitBoxが有効なまま次のStateへ
	// 遷移してしまうと、以後の状態(Stagger中など)でも攻撃判定が
	// 生き続けてしまう。通常のUpdate側の遷移(Active→Recovery)で
	// 既に無効化済みのケースがほとんどだが、その経路を通らない
	// 中断にも安全に対応できるよう、Exitで無条件に無効化しておく。
	controller->SetWeaponHitBoxEnabled(data.weaponSlots, false);

	// HitBoxと同じ理由で、AttackActive中に割り込まれた場合でも
	// トレイルの記録が停止せずに残ってしまわないよう、無条件で止める。
	controller->SetWeaponTrailEmitting(data.weaponSlots, false);
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

bool StateAttack::CanStartGuard(const PlayerStatusController* controller) const {
	if (phase_ == CombatState::AttackRecovery) {
		//return elapsed_ >= controller->GetCurrentAttackData().recoveryEvadeCancelStart;
		return true;
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
	//if (!data.useRootMotion) {
	controller->RequestStepMove(data.evadeDirection, data.evadeDistance, data.activeDuration + data.recoveryDuration);
	//}
}

void StateEvade::Exit(PlayerStatusController* controller) {
	// EvadeRecovery終了(あるいは何らかの理由での中断)で必ず後始末する。
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
	parrySucceeded_ = false;
	parrySuccessElapsed_ = 0.0f;

	// 単発再生(loop=false)にすることで「構えに入る動作を1回再生し、
	// 最終フレームでポーズを保持する」形にする。
	controller->PlayAnimation(controller->GetCurrentGuardData().animationName, false, controller->GetCurrentGuardData().guardTransitionDuration);
}

void StateGuard::Update(PlayerStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;
	// Guardは継続状態なので、時間経過による自動終了はない

	if (parrySucceeded_) {
		parrySuccessElapsed_ += deltaTime;
		if (parrySuccessElapsed_ >= controller->GetCurrentGuardData().parrySuccessDuration) {
			// 演出終了。NormalBlockへ復帰する(ガードキーが既に離されていれば
			// 次フレームのCanReleaseGuard()判定でHandleActionInput側が解除する)。
			parrySucceeded_ = false;
		}
	}
}

bool StateGuard::IsInParryWindow(const PlayerStatusController* controller) const {
	return GetGuardPhase(controller) == GuardPhase::JustWindow;
}

bool StateGuard::CanReleaseGuard(const PlayerStatusController* controller) const {
	// パリィ成功演出中は強制的に見せ切る(ガードキーを離しても解除しない)。
	return !parrySucceeded_;
}

bool StateGuard::CanStartAttack(const PlayerStatusController* controller) const {
	// パリィ成功演出中のみ、反撃キャンセルとして次の攻撃を許可する。
	return parrySucceeded_;
}

void StateGuard::NotifyParrySuccess(PlayerStatusController* controller) {
	if (parrySucceeded_) return; // 同一パリィ猶予内での多重成立を防止
	parrySucceeded_ = true;
	parrySuccessElapsed_ = 0.0f;
	controller->PlayAnimation(controller->GetCurrentGuardData().parrySuccessAnimationName, false, controller->GetCurrentGuardData().parrySuccessDuration);
}

void StateGuard::NotifyGuardHit(PlayerStatusController* controller) {
	// パリィ成功と異なり内部フェーズは変えず、都度ヒットリアクションだけ
	// 再生する(ガード解除可否・反撃キャンセル可否には影響させない)。
	controller->PlayAnimation(controller->GetCurrentGuardData().guardHitAnimationName, false, controller->GetCurrentGuardData().guardHitDuration);
}

StateGuard::GuardPhase StateGuard::GetGuardPhase(const PlayerStatusController* controller) const {
	if (parrySucceeded_) return GuardPhase::ParrySuccess;
	return elapsed_ <= controller->GetCurrentGuardData().justWindowDuration
		? GuardPhase::JustWindow
		: GuardPhase::NormalBlock;
}

// --- Stagger State ---
void StateStagger::Enter(PlayerStatusController* controller) {
	elapsed_ = 0.0f;

	// アニメーション未実装のためコメントアウト。
	// AttackMoveData/GuardMoveDataのような専用データ構造をStaggerは
	// 持たないため、isLarge_で仮のアニメーション名を直接出し分ける想定だった。
	controller->PlayAnimation(isLarge_ ? "GhostSamurai_APose_Large_Hit_Inplace" : "GhostSamurai_APose_Hit_B_Inplace");
}

void StateStagger::Update(PlayerStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;
	if (elapsed_ >= duration_) {
		controller->ChangeStateToNone();
	}
}

void StateStagger::Exit(PlayerStatusController* controller)
{
}