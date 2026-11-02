// PlayerActionDispatcher.cpp
#include "PlayerActionDispatcher.h"

namespace
{
	struct Rule
	{
		PlayerInputCommand command;
		PlayerAction action;
	};

	// 同フレームに複数成立した場合の優先順。変更はここだけ。
	constexpr Rule kPriority[] = {
		{ PlayerInputCommand::Evade,  PlayerAction::Evade },
		{ PlayerInputCommand::Charge, PlayerAction::Charge },
		{ PlayerInputCommand::Attack, PlayerAction::Attack },
		{ PlayerInputCommand::Guard,  PlayerAction::Guard },
	};
}

void PlayerActionDispatcher::Dispatch(InputBuffer<PlayerInputCommand>& buffer,
	const PlayerIntent& intent, IPlayerActionTarget& target) const
{
	// ロック切替は戦闘状態に依らず常に処理する。
	if (buffer.Consume(PlayerInputCommand::LockToggle)) {
		if (target.IsLockedOn()) target.ClearLockOn();
		else target.TryLockOn();
	}

	if (TryStartBuffered(buffer, target)) return;

	if (!intent.guardHeld && target.GetCombatState() == CombatState::Guard && target.CanReleaseGuard()) {
		target.RequestGuardRelease();
		return;
	}
}

bool PlayerActionDispatcher::TryStartBuffered(InputBuffer<PlayerInputCommand>& buffer,
	IPlayerActionTarget& target) const
{
	for (const Rule& rule : kPriority) {
		Math::Vector3 direction = Math::Vector3::Zero;
		if (!buffer.Peek(rule.command, &direction)) continue;
		if (!target.Allows(rule.action)) continue;
		if (!target.TryStart(rule.action, direction)) continue;

		buffer.Consume(rule.command);
		return true;
	}
	return false;
}
