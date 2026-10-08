#pragma once

#include "Event.h"
#include <string>

namespace Events
{
	struct SlashTrailBeginEvent : public Event
	{
		std::string InstanceKey;
		std::string TrailName;
	};

	struct SlashTrailPositionUpdateEvent : public Event
	{
		std::string   InstanceKey;
		Math::Vector3 Tip;
		Math::Vector3 Base;
	};

	struct SlashTrailEndEvent : public Event
	{
		std::string InstanceKey;
	};

	// Publishヘルパー
	inline void PublishSlashTrailBegin(EventBus& bus, const std::string& instanceKey, const std::string& trailName)
	{
		SlashTrailBeginEvent e;
		e.InstanceKey = instanceKey;
		e.TrailName = trailName;

		bus.Publish(e);
	}

	inline void PublishSlashTrailPositionUpdate(EventBus& bus, const std::string& instanceKey,
		const Math::Vector3& tip, const Math::Vector3& base)
	{
		SlashTrailPositionUpdateEvent e;
		e.InstanceKey = instanceKey;
		e.Tip = tip;
		e.Base = base;

		bus.Publish(e);
	}

	inline void PublishSlashTrailEnd(EventBus& bus, const std::string& instanceKey)
	{
		SlashTrailEndEvent e;
		e.InstanceKey = instanceKey;

		bus.Publish(e);
	}
}