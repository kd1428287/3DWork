#include "WarrockActions.h"
#include "WarrockAIController.h"

// --- WarrockActionIdle ---
BTNodeStatus WarrockActionIdle::Tick(WarrockAIController* context, float deltaTime)
{
	// 持ち場から動かず、その場でIdleループアニメーションを再生し続ける
	// だけの実装(クラスコメント参照)。単発アニメーション(Attack/Roar/
	// HitReaction等)は再生後アニメーターが最終フレームで止まったまま
	// になるが、reactiveなSelectorが毎フレーム評価し直し、他の分岐が
	// 全てFailureになった瞬間にこのIdleへフォールバックしてくるため、
	// ここでPlayAnimationIfChanged()するだけで自然に固まりが解消される。
	//
	// 【将来の再利用について】各攻撃(Punch/Kick/Swipe/JumpAttack)の
	// 合間の「間(ま)」としても、このActionをそのまま再利用する想定
	// (WarrockActionAttackのクラスコメント参照)。今はattackSeq側に
	// クールダウンが無いため、間合い内にターゲットがいる限りRecovery
	// 終了直後にattackSeqが再び選ばれ、実質このIdleを経由しない。
	// 攻撃間隔を空けたくなったら、attackSeq(とchaseSeq)の条件に
	// 「クールダウン中はFailureを返す」判定を足し、両方失敗した際に
	// ここへ落ちてくるように設計すること。
	context->StopMovement();
	context->PlayAnimationIfChanged("Idle", true);
	return BTNodeStatus::Running;
}
// --- WarrockActionChase ---
BTNodeStatus WarrockActionChase::Tick(WarrockAIController* context, float /*deltaTime*/)
{
	if (!context->HasTarget()) return BTNodeStatus::Failure;

	Math::Vector3 toTarget = context->GetTargetPositionOrSelf() - context->GetPosition();
	toTarget.y = 0.0f;

	if (toTarget.LengthSquared() < 1e-6f) {
		context->StopMovement();
		return BTNodeStatus::Running;
	}

	toTarget.Normalize();
	context->SetDesiredVelocity(toTarget * context->GetData().chaseSpeed);
	context->PlayAnimationIfChanged("Run", true);
	return BTNodeStatus::Running;
}

// --- WarrockActionAttack ---
BTNodeStatus WarrockActionAttack::Tick(WarrockAIController* context, float deltaTime)
{
	lastContext_ = context; // Reset()からの後始末用にキャッシュ(ヘッダのコメント参照)

	if (phase_ == Phase::NotStarted) {
		current_ = context->ChooseAttack();
		if (current_ == nullptr) return BTNodeStatus::Failure;

		phase_ = Phase::Windup;
		elapsed_ = 0.0f;

		context->StopMovement();
		context->FaceHorizontalTarget(context->GetTargetPositionOrSelf());

		// 攻撃全体(Windup+Active+Recovery)の秒数を目標としてアニメーション
		// 速度を自動スケーリングする(Player/EnemyのPlayAnimationと同じ考え方)。
		const float totalDuration = current_->windupDuration + current_->activeDuration + current_->recoveryDuration;
		context->PlayAnimation(current_->animationName, false, totalDuration);
	}

	elapsed_ += deltaTime;

	switch (phase_) {
	case Phase::Windup:
		if (elapsed_ >= current_->windupDuration) {
			phase_ = Phase::Active;
			elapsed_ = 0.0f;
			context->SetWeaponHitBoxEnabled(true); // 攻撃判定が実際に発生する一瞬だけ有効化
		}
		break;

	case Phase::Active:
		if (elapsed_ >= current_->activeDuration) {
			phase_ = Phase::Recovery;
			elapsed_ = 0.0f;
			context->SetWeaponHitBoxEnabled(false); // 判定の発生窓を閉じる
		}
		break;

	case Phase::Recovery:
		if (elapsed_ >= current_->recoveryDuration) {
			Reset();
			return BTNodeStatus::Success;
		}
		break;

	default:
		break;
	}

	return BTNodeStatus::Running;
}

void WarrockActionAttack::Reset()
{
	// Active中(HitBoxが有効な最中)に中断された場合は、有効なまま残らない
	// よう明示的に閉じておく(ヘッダのコメント参照)。
	if (phase_ == Phase::Active && lastContext_ != nullptr) {
		lastContext_->SetWeaponHitBoxEnabled(false);
	}

	phase_ = Phase::NotStarted;
	elapsed_ = 0.0f;
	current_ = nullptr;
}

// --- WarrockActionHitReaction ---
BTNodeStatus WarrockActionHitReaction::Tick(WarrockAIController* context, float deltaTime)
{
	if (!playing_) {
		playing_ = true;
		elapsed_ = 0.0f;
		context->StopMovement();
		context->PlayAnimation("SmallReaction", false, kReactionDuration);
	}

	elapsed_ += deltaTime;
	if (elapsed_ >= kReactionDuration) {
		playing_ = false;
		context->ConsumeHitReactionRequest();
		return BTNodeStatus::Success;
	}
	return BTNodeStatus::Running;
}

// --- WarrockActionRoar ---
BTNodeStatus WarrockActionRoar::Tick(WarrockAIController* context, float deltaTime)
{
	if (!playing_) {
		playing_ = true;
		elapsed_ = 0.0f;
		context->StopMovement();
		context->PlayAnimation("Roaring", false, kRoarDuration);
	}

	elapsed_ += deltaTime;
	if (elapsed_ >= kRoarDuration) {
		playing_ = false;
		context->ConsumeRoarRequest();
		return BTNodeStatus::Success;
	}
	return BTNodeStatus::Running;
}