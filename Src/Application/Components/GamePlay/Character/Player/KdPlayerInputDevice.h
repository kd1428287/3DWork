#pragma once
#include "PlayerInputDevice.h"

namespace PlayerInputNames
{
	constexpr const char* Move = "Move";
	constexpr const char* Dash = "Dash";
	constexpr const char* Guard = "Guard";
	constexpr const char* Attack = "Attack";
	constexpr const char* Evade = "Evade";
	constexpr const char* Lock = "Lock";
}

class KdPlayerInputDevice : public IPlayerInputDevice
{
public:
	PlayerRawInput Poll() override
	{
		KdInputManager& in = KdInputManager::Instance();
		PlayerRawInput raw;
		raw.moveAxis = in.GetAxisState(PlayerInputNames::Move);
		raw.dashHeld = in.IsHold(PlayerInputNames::Dash);
		raw.guardHeld = in.IsHold(PlayerInputNames::Guard);
		raw.attackHeld = in.IsHold(PlayerInputNames::Attack);
		raw.attackPressed = in.IsPress(PlayerInputNames::Attack);
		raw.evadePressed = in.IsPress(PlayerInputNames::Evade);
		raw.guardPressed = in.IsPress(PlayerInputNames::Guard);
		raw.lockPressed = in.IsPress(PlayerInputNames::Lock);
		return raw;
	}
};
