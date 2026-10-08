#pragma once

#include "Event.h"

class GameObject;

namespace Events
{
	// HPが変化した(ダメージ/回復/SetCurrent/SetMax等)
	struct HealthChangedEvent : public Event {
		Handle<GameObject> source; // 誰のHPが変化したか
		float ratio = 1.0f;        // 変化後の割合(0.0～1.0)
	};

	// HPが0になった(死亡)
	struct HealthDiedEvent : public Event {
		Handle<GameObject> source;
	};

	inline void PublishHealthChangeEvent(EventBus& bus, Handle<GameObject> obj, float ratio)
	{
		HealthChangedEvent e;
		e.source = obj;
		e.ratio = ratio;
		bus.Publish(e);
	}
}