#pragma once

#include "Event.h"

class GameObject;

namespace Events
{
	// 体幹値が変化した(被弾/自然回復/リセット時)
	struct PostureChangedEvent : public Event {
		Handle<GameObject> source;
		float ratio = 0.0f;
	};
}