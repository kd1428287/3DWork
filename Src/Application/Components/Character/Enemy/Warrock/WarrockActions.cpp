#include "WarrockActions.h"
#include "../EnemyAIController.h"

// --- WarrockActionIdle ---
BTNodeStatus WarrockActionIdle::Tick(EnemyAIController* context, float /*deltaTime*/)
{
	// 持ち場から動かず、その場でIdleループアニメーションを再生し続ける
	// だけの実装(クラスコメント参照)。単発アニメーション(攻撃/咆哮/
	// 被弾リアクション等)は再生後アニメーターが最終フレームで止まったまま
	// になるが、reactiveなSelectorが毎フレーム評価し直し、他の分岐が
	// 全てFailureになった瞬間にこのIdleへフォールバックしてくるため、
	// ここでPlayAnimationIfChanged()するだけで自然に固まりが解消される。
	context->StopMovement();
	context->PlayAnimationIfChanged("Idle", true);
	return BTNodeStatus::Running;
}

// --- WarrockActionChase ---
BTNodeStatus WarrockActionChase::Tick(EnemyAIController* context, float /*deltaTime*/)
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
