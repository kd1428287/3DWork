#include "EnemyStatusController.h"

// --- StateWalkRight ---
void StateWalkRight::Enter(EnemyStatusController* controller) {
	controller->SetDesiredDirection({ 1.0f, 0.0f, 0.0f });
}

void StateWalkRight::Update(EnemyStatusController* controller, float /*deltaTime*/) {
	const float traveled = controller->GetCurrentPosition().x - controller->GetBasePosition().x;
	if (traveled >= controller->GetPatrolDistance()) {
		controller->ChangeStateToWalkLeft();
	}
}

// --- StateWalkLeft ---
void StateWalkLeft::Enter(EnemyStatusController* controller) {
	controller->SetDesiredDirection({ -1.0f, 0.0f, 0.0f });
}

void StateWalkLeft::Update(EnemyStatusController* controller, float /*deltaTime*/) {
	const float traveled = controller->GetBasePosition().x - controller->GetCurrentPosition().x;
	if (traveled >= controller->GetPatrolDistance()) {
		controller->ChangeStateToWalkRight();
	}
}

// --- StateKnockback ---
// 吹っ飛ばし自体はVelocityComponent::AddImpulse()に一度だけ委譲する
// (毎フレーム位置を上書きするのではなく、物理的な減衰にそのまま任せる。
//  理由はEnemyState.hのKnockbackParamsコメント参照)。
// このStateが担うのは「いつパトロールへ復帰してよいか」の判定だけ。
void StateKnockback::Enter(EnemyStatusController* controller) {
	elapsed_ = 0.0f;
	controller->ApplyKnockbackImpulse(params_.direction * params_.power);
}

void StateKnockback::Update(EnemyStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;

	// 最低保証時間が経過していない間は、物理的に止まっていても
	// まだ怯み演出を継続する(演出上不自然に短い怯みを防ぐ)。
	if (elapsed_ < params_.minStunDuration) return;

	// 物理的にまだ吹っ飛び中(VelocityComponentの摩擦減衰がまだ閾値を
	// 上回っている)なら、パトロールへ戻さずそのまま待つ。ここで
	// SetDesiredDirection等による位置の手動書き換えは一切行わない
	// (VelocityComponent::Update()が毎フレーム自動で位置を進める)。
	if (controller->IsKnockbackImpulseActive()) return;

	// 怯み終了。基準点から見て今どちら側にいるかで、自然に続きの
	// パトロールへ戻す(元々居た側の反対へ歩き出すと不自然なため)。
	const float traveled = controller->GetCurrentPosition().x - controller->GetBasePosition().x;
	if (traveled >= 0.0f) {
		controller->ChangeStateToWalkRight();
	}
	else {
		controller->ChangeStateToWalkLeft();
	}
}

void StateKnockback::Exit(EnemyStatusController* /*controller*/) {
}