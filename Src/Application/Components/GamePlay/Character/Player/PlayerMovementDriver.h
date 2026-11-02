// PlayerMovementDriver.h
#pragma once
#include "PlayerStateContext.h"
#include "PlayerActionDispatcher.h"

// 移動入力の反映(移動状態の更新、向き、移動アニメ、戦闘からの移動キャンセル)。
class PlayerMovementDriver
{
public:
	void Tick(PlayerStateContext& ctx, const PlayerIntent& intent,
		IPlayerActionTarget& target, float deltaTime) const;
};
