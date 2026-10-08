#pragma once

#include "Event.h"
#include <string>

namespace Events
{
	struct GenericEffectSpawnEvent : public Event
	{
		std::string   Id;
		Math::Vector3 Position;
		Math::Vector3 Direction;
	};

	struct EffectAttachSpawnEvent : public Event
	{
		std::string   InstanceKey;
		std::string   EffectName;
		Math::Vector3 Position;
	};

	struct EffectPositionUpdateEvent : public Event
	{
		std::string   InstanceKey;
		Math::Vector3 Position;
	};

	struct EffectDetachEvent : public Event
	{
		std::string InstanceKey;
	};

	// Publishヘルパー
	inline void PublishGenericEffect(EventBus& bus, const std::string& id, const Math::Vector3& pos, const Math::Vector3& dir = Math::Vector3::Zero)
	{
		GenericEffectSpawnEvent e;
		e.Id = id;
		e.Position = pos;
		e.Direction = dir;
		e.Direction.Normalize();

		bus.Publish(e);
	}

	inline void PublishEffectAttach(EventBus& bus, const std::string& instanceKey,
		const std::string& effectName, const Math::Vector3& pos)
	{
		EffectAttachSpawnEvent e;
		e.InstanceKey = instanceKey;
		e.EffectName = effectName;
		e.Position = pos;

		bus.Publish(e);
	}

	inline void PublishEffectPositionUpdate(EventBus& bus, const std::string& instanceKey, const Math::Vector3& pos)
	{
		EffectPositionUpdateEvent e;
		e.InstanceKey = instanceKey;
		e.Position = pos;

		bus.Publish(e);
	}

	inline void PublishEffectDetach(EventBus& bus, const std::string& instanceKey)
	{
		EffectDetachEvent e;
		e.InstanceKey = instanceKey;

		bus.Publish(e);
	}
}