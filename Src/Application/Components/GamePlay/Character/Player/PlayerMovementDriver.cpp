// PlayerMovementDriver.cpp
#include "PlayerMovementDriver.h"

#include "PlayerFacingComponent.h"
#include "PlayerCombatMovementComponent.h"
#include "PlayerMovementAnimationComponent.h"

void PlayerMovementDriver::Tick(PlayerStateContext& ctx, const PlayerIntent& intent,
	IPlayerActionTarget& target, float deltaTime) const
{
	const MovementState nextState = intent.DesiredMovementState();

	if (target.GetCombatState() != CombatState::None) {
		// 後隙などで移動キャンセルが許可され、実際に移動入力がある場合のみNoneへ戻る。
		if (!target.Allows(PlayerAction::Move) || nextState == MovementState::Stand) return;
		target.ChangeStateToNone();
	}

	if (ctx.movementState != nextState) {
		ctx.movementState = nextState;
		if (ctx.combatMovement != nullptr) {
			ctx.combatMovement->ApplyMovementState(ctx.movementState);
		}
	}

	const bool lockedOn = ctx.IsLockedOn();
	if (ctx.facing != nullptr) {
		ctx.facing->SetFacingEnabled(!lockedOn || ctx.movementState == MovementState::Run);
	}
	if (ctx.movementAnimation != nullptr) {
		ctx.movementAnimation->Tick(deltaTime, ctx.movementState, intent.moveDirection, lockedOn);
	}
}
