// PlayerActionDispatcher.h
#pragma once
#include "Application/Definitions/Character/Player/PlayerInputTypes.h"
#include "Application/Entity/InputBuffer.h"

// ディスパッチャが操作する対象(PlayerStatusControllerが実装する)。
class IPlayerActionTarget
{
public:
	virtual ~IPlayerActionTarget() = default;

	virtual CombatState GetCombatState() const = 0;
	virtual bool Allows(PlayerAction action) const = 0;
	virtual bool TryStart(PlayerAction action, const Math::Vector3& direction) = 0;

	virtual bool CanReleaseGuard() const = 0;
	virtual void RequestGuardRelease() = 0;
	virtual void ChangeStateToNone() = 0;

	virtual bool IsLockedOn() const = 0;
	virtual void TryLockOn() = 0;
	virtual void ClearLockOn() = 0;
};

// バッファ内のコマンドを優先順位表に従って実行する。成功した時だけ消費する。
class PlayerActionDispatcher
{
public:
	void Dispatch(InputBuffer<PlayerInputCommand>& buffer, const PlayerIntent& intent,
		IPlayerActionTarget& target) const;

private:
	bool TryStartBuffered(InputBuffer<PlayerInputCommand>& buffer, IPlayerActionTarget& target) const;
};
