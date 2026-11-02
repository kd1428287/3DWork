// PlayerInputDevice.h
#pragma once
#include "Application/Definitions/Character/Player/PlayerInputTypes.h"

// 生入力の取得元。リプレイ・テスト・AI操作への差し替え口。
class IPlayerInputDevice
{
public:
	virtual ~IPlayerInputDevice() = default;
	virtual PlayerRawInput Poll() = 0;
};
