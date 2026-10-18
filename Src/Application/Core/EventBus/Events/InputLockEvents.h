#pragma once

#include "Event.h"

namespace Events
{
	// 操作・視点入力のロック切り替え(リザルト演出中など)。解除はlocked=falseを流す
	struct InputLockChanged : public Event
	{
		bool locked = true;
	};
}
