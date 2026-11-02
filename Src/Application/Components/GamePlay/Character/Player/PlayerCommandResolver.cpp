// PlayerCommandResolver.cpp
#include "PlayerCommandResolver.h"

void PlayerCommandResolver::Update(float deltaTime, const PlayerRawInput& raw,
	const Math::Vector3& moveDir, const Math::Vector3& evadeDir, Buffer& buffer)
{
	buffer.Tick(deltaTime);
	attackPressAge_ += deltaTime;

	// Attackが未消費のまま猶予内にGuardが来た場合も同時押しとする。
	const bool attackPending = buffer.Has(PlayerInputCommand::Attack) && attackPressAge_ <= kChordWindow;
	const bool chord = (raw.attackPressed && raw.guardHeld) || (raw.guardPressed && attackPending);

	if (chord) {
		buffer.Clear(PlayerInputCommand::Attack);
		buffer.Clear(PlayerInputCommand::Guard);
		buffer.Push(PlayerInputCommand::Charge, Math::Vector3::Zero, kInputBufferWindow);
	}
	else {
		if (raw.attackPressed) buffer.Push(PlayerInputCommand::Attack, moveDir, kInputBufferWindow);
		if (raw.guardPressed) buffer.Push(PlayerInputCommand::Guard, Math::Vector3::Zero, kInputBufferWindow);
	}
	if (raw.attackPressed) attackPressAge_ = 0.0f;

	if (raw.evadePressed) buffer.Push(PlayerInputCommand::Evade, evadeDir, kInputBufferWindow);
	if (raw.lockPressed) buffer.Push(PlayerInputCommand::LockToggle, Math::Vector3::Zero, kLockToggleWindow);

	// ガードを離したら、未消費のGuard/Chargeは意味を失うので捨てる。
	if (!raw.guardHeld) {
		buffer.Clear(PlayerInputCommand::Guard);
		buffer.Clear(PlayerInputCommand::Charge);
	}
}
