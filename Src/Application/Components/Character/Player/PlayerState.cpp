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

	// 具体的なコンポーネント操作(Transform/TweenMoveComponent/
	// ModelAnimatorComponent)はController側に閉じ込め、Stateは
	// それを直接知らなくてよいようにする。
	const auto& data = controller->GetCurrentAttackData();
	// 攻撃全体(Windup+Active+Recovery)の秒数を目標としてアニメーション
	// 速度を自動スケーリングする(詳細はModelAnimatorComponent::Play参照)。
	const float targetDuration = data.windupDuration + data.activeDuration + data.recoveryDuration;
	controller->PlayAnimation(data.animationName, false, targetDuration, data.blendDuration); // コンボ段数に応じたアニメーション

	// 踏み込み移動はここ(Windup開始時点)では行わない。Windupが終わった
	// 瞬間(Update()側、AttackActiveへの切り替わり)に開始する
	// (振りかぶり中に前進してしまうと予備動作の説得力が薄れるため)。

	KdDebugGUI::Instance().AddLog("AttackWindup"); // 必要なら
}

void StateAttack::Update(PlayerStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;
	const auto& data = controller->GetCurrentAttackData();

	KdDebugGUI::Instance().AddLog("Attack");

	if (phase_ == CombatState::AttackWindup && elapsed_ >= data.windupDuration) {
		phase_ = CombatState::AttackActive;
		elapsed_ = 0.0f;

		// 踏み込み移動はここ(Windupが終わった瞬間)から開始する。
		// 移動時間はwindupDurationではなく専用のstepDurationを使う
		// (以前はEnter()側でwindupDuration分だけ振りかぶり中に動かして
		//  いたが、攻撃が実際に届き始めるタイミングと踏み込みを
		//  合わせたいという理由でここへ移した)。
		controller->RequestStepMove(data.stepDirection, data.stepDistance, data.stepDuration);
		controller->SetWeaponHitBoxEnabled(true); // 攻撃判定が実際に発生する一瞬だけ有効化
		KdDebugGUI::Instance().AddLog("\nAttackActive");
	}
	else if (phase_ == CombatState::AttackActive && elapsed_ >= data.activeDuration) {
		phase_ = CombatState::AttackRecovery;
		elapsed_ = 0.0f;
		controller->SetWeaponHitBoxEnabled(false); // 判定の発生窓を閉じる
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

	// AttackActive中に割り込まれた場合、HitBoxが有効なまま次のStateへ
	// 遷移してしまうと、以後の状態(Stagger中など)でも攻撃判定が
	// 生き続けてしまう。通常のUpdate側の遷移(Active→Recovery)で
	// 既に無効化済みのケースがほとんどだが、その経路を通らない
	// 中断にも安全に対応できるよう、Exitで無条件に無効化しておく。
	controller->SetWeaponHitBoxEnabled(false);
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
	KdDebugGUI::Instance().AddLog("Evade");

	// 回避中の移動は入力ではなく、決め打ちの軌道(RequestStepMove)に任せる。
	// MovementComponentはTransitionTo側で既に無効化されているため、
	// 位置を書き換える権利がここで競合することはない。
	const auto& data = controller->GetCurrentEvadeData();
	// 回避全体(Active+Recovery)の秒数を目標としてアニメーション速度を
	// 自動スケーリングする(詳細はModelAnimatorComponent::Play参照)。
	const float targetDuration = data.activeDuration + data.recoveryDuration;
	controller->PlayAnimation(data.animationName, false, targetDuration);
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

bool StateEvade::IsInvincible(const PlayerStatusController* controller) const {
	// Evade(実移動フェーズ)中のみ無敵。EvadeRecoveryは無敵切れとして
	// 通常の被弾判定に戻す(後隙に攻撃を合わせられたら普通に食らう)。
	return phase_ == CombatState::Evade;
}


// --- Guard State ---
void StateGuard::Enter(PlayerStatusController* controller) {
	elapsed_ = 0.0f;
	KdDebugGUI::Instance().AddLog("Guard");

	// 単発再生(loop=false)にすることで「構えに入る動作を1回再生し、
	// 最終フレームでポーズを保持する」形にする。
	controller->PlayAnimation(controller->GetCurrentGuardData().animationName, false);
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

	// アニメーション未実装のためコメントアウト。
	// AttackMoveData/GuardMoveDataのような専用データ構造をStaggerは
	// 持たないため、isLarge_で仮のアニメーション名を直接出し分ける想定だった。
	controller->PlayAnimation(isLarge_ ? "miniStun" : "miniStun");
}

void StateStagger::Update(PlayerStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;
	if (elapsed_ >= duration_) {
		controller->ChangeStateToNone();
	}
}