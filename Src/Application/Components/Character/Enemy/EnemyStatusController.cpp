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
