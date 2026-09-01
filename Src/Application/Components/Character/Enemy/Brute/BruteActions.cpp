//#include "BruteActions.h"
//#include "BruteAIController.h"
//
//// --- BruteActionIdle ---
//BTNodeStatus BruteActionIdle::Tick(BruteAIController* context, float deltaTime)
//{
//	context->StopMovement();
//	context->PlayAnimationIfChanged("Idle", true);
//
//	// 巡回地点が無ければ巡回へ進む意味が無いため、待機のまま居座る。
//	if (context->GetData().patrolPoints.empty()) {
//		return BTNodeStatus::Running;
//	}
//
//	elapsed_ += deltaTime;
//	if (elapsed_ >= context->GetData().idleDuration) {
//		elapsed_ = 0.0f;
//		return BTNodeStatus::Success;
//	}
//	return BTNodeStatus::Running;
//}
//
//// --- BruteActionPatrol ---
//BTNodeStatus BruteActionPatrol::Tick(BruteAIController* context, float /*deltaTime*/)
//{
//	if (context->GetData().patrolPoints.empty()) return BTNodeStatus::Failure;
//
//	const Math::Vector3 target = context->GetCurrentPatrolPoint();
//	Math::Vector3 toTarget = target - context->GetPosition();
//	toTarget.y = 0.0f;
//
//	if (toTarget.LengthSquared() <= kArriveThreshold * kArriveThreshold) {
//		context->StopMovement();
//		context->AdvanceToNextPatrolPoint();
//		return BTNodeStatus::Success;
//	}
//
//	toTarget.Normalize();
//	context->SetDesiredVelocity(toTarget * context->GetData().patrolSpeed);
//	context->PlayAnimationIfChanged("Walk", true);
//	return BTNodeStatus::Running;
//}
//
//// --- BruteActionChase ---
//BTNodeStatus BruteActionChase::Tick(BruteAIController* context, float /*deltaTime*/)
//{
//	if (!context->HasTarget()) return BTNodeStatus::Failure;
//
//	Math::Vector3 toTarget = context->GetTargetPositionOrSelf() - context->GetPosition();
//	toTarget.y = 0.0f;
//
//	if (toTarget.LengthSquared() < 1e-6f) {
//		context->StopMovement();
//		return BTNodeStatus::Running;
//	}
//
//	toTarget.Normalize();
//	context->SetDesiredVelocity(toTarget * context->GetData().chaseSpeed);
//	context->PlayAnimationIfChanged("Run", true);
//	return BTNodeStatus::Running;
//}
//
//// --- BruteActionAttack ---
//BTNodeStatus BruteActionAttack::Tick(BruteAIController* context, float deltaTime)
//{
//	lastContext_ = context; // Reset()からの後始末用にキャッシュ(ヘッダのコメント参照)
//
//	if (phase_ == Phase::NotStarted) {
//		current_ = context->ChooseAttack();
//		if (current_ == nullptr) return BTNodeStatus::Failure;
//
//		phase_ = Phase::Windup;
//		elapsed_ = 0.0f;
//
//		context->StopMovement();
//		context->FaceHorizontalTarget(context->GetTargetPositionOrSelf());
//
//		// 攻撃全体(Windup+Active+Recovery)の秒数を目標としてアニメーション
//		// 速度を自動スケーリングする(Player/EnemyのPlayAnimationと同じ考え方)。
//		const float totalDuration = current_->windupDuration + current_->activeDuration + current_->recoveryDuration;
//		context->PlayAnimation(current_->animationName, false, totalDuration);
//	}
//
//	elapsed_ += deltaTime;
//
//	switch (phase_) {
//	case Phase::Windup:
//		if (elapsed_ >= current_->windupDuration) {
//			phase_ = Phase::Active;
//			elapsed_ = 0.0f;
//			context->SetWeaponHitBoxEnabled(true); // 攻撃判定が実際に発生する一瞬だけ有効化
//		}
//		break;
//
//	case Phase::Active:
//		if (elapsed_ >= current_->activeDuration) {
//			phase_ = Phase::Recovery;
//			elapsed_ = 0.0f;
//			context->SetWeaponHitBoxEnabled(false); // 判定の発生窓を閉じる
//		}
//		break;
//
//	case Phase::Recovery:
//		if (elapsed_ >= current_->recoveryDuration) {
//			Reset();
//			return BTNodeStatus::Success;
//		}
//		break;
//
//	default:
//		break;
//	}
//
//	return BTNodeStatus::Running;
//}
//
//void BruteActionAttack::Reset()
//{
//	// Active中(HitBoxが有効な最中)に中断された場合は、有効なまま残らない
//	// よう明示的に閉じておく(ヘッダのコメント参照)。
//	if (phase_ == Phase::Active && lastContext_ != nullptr) {
//		lastContext_->SetWeaponHitBoxEnabled(false);
//	}
//
//	phase_ = Phase::NotStarted;
//	elapsed_ = 0.0f;
//	current_ = nullptr;
//}
