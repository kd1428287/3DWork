// PlayerInputTypes.h
#pragma once
#include "Application/Definitions/Character/Player/PlayerCombatTypes.h"

// バッファに積む入力コマンド。技のエントリキー(ActionCommand)とは別物。
enum class PlayerInputCommand
{
	Attack,
	Evade,
	Guard,
	Charge, // Attack+Guardの同時押し(入力層で合成する)
	LockToggle,
};

// Stateへ「今始めてよいか」を問い合わせる行動の種類。
enum class PlayerAction
{
	Attack,
	Evade,
	Guard,
	Charge,
	Move,
};

constexpr float kInputBufferWindow = 0.2f;  // 先行入力の猶予(秒)
constexpr float kLockToggleWindow = 0.1f;
constexpr float kChordWindow = 0.06f;       // この秒数以内の押し分けは同時押しとみなす

// デバイスから読んだ今フレームの生入力。
struct PlayerRawInput
{
	Math::Vector2 moveAxis;
	bool dashHeld = false;
	bool guardHeld = false;
	bool attackHeld = false;
	bool attackPressed = false;
	bool evadePressed = false;
	bool guardPressed = false;
	bool lockPressed = false;
};

// 保持系の入力と移動意思(押下イベントはバッファ側が持つ)。
struct PlayerIntent
{
	Math::Vector3 moveDirection;
	bool dashHeld = false;
	bool guardHeld = false;
	bool attackHeld = false;

	bool ChargeHeld() const { return attackHeld && guardHeld; }

	MovementState DesiredMovementState() const
	{
		if (moveDirection.LengthSquared() <= 0.0f) return MovementState::Stand;
		return dashHeld ? MovementState::Run : MovementState::Walk;
	}
};
