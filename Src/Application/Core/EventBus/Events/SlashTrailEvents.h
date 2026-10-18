#pragma once

#include "Event.h"
#include <string>

namespace Events
{
	struct SlashTrailBegin : public Event
	{
		std::string InstanceKey;
		std::string TrailName;
	};

	struct SlashTrailPositionUpdate : public Event
	{
		std::string   InstanceKey;
		Math::Vector3 Tip;
		Math::Vector3 Base;
	};

	struct SlashTrailEnd : public Event
	{
		std::string InstanceKey;
	};

	// Publishヘルパー
	inline void PublishSlashTrailBegin(EventBus& bus, const std::string& instanceKey, const std::string& trailName)
	{
		SlashTrailBegin e;
		e.InstanceKey = instanceKey;
		e.TrailName = trailName;

		bus.Publish(e);
	}

	inline void PublishSlashTrailPositionUpdate(EventBus& bus, const std::string& instanceKey,
		const Math::Vector3& tip, const Math::Vector3& base)
	{
		SlashTrailPositionUpdate e;
		e.InstanceKey = instanceKey;
		e.Tip = tip;
		e.Base = base;

		bus.Publish(e);
	}

	inline void PublishSlashTrailEnd(EventBus& bus, const std::string& instanceKey)
	{
		SlashTrailEnd e;
		e.InstanceKey = instanceKey;

		bus.Publish(e);
	}
}