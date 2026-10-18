#pragma once

#include "Event.h"
#include <string>

namespace Events
{
	struct GenericEffectSpawn : public Event
	{
		std::string   Id;
		Math::Vector3 Position;
		Math::Vector3 Direction;
	};

	struct EffectAttachSpawn : public Event
	{
		std::string   InstanceKey;
		std::string   EffectName;
		Math::Vector3 Position;
	};

	struct EffectPositionUpdate : public Event
	{
		std::string   InstanceKey;
		Math::Vector3 Position;
	};

	struct EffectDetach : public Event
	{
		std::string InstanceKey;
	};

	// Publishヘルパー
	inline void PublishGenericEffect(EventBus& bus, const std::string& id, const Math::Vector3& pos, const Math::Vector3& dir = Math::Vector3::Zero)
	{
		GenericEffectSpawn e;
		e.Id = id;
		e.Position = pos;
		e.Direction = dir;
		e.Direction.Normalize();

		bus.Publish(e);
	}

	inline void PublishEffectAttach(EventBus& bus, const std::string& instanceKey,
		const std::string& effectName, const Math::Vector3& pos)
	{
		EffectAttachSpawn e;
		e.InstanceKey = instanceKey;
		e.EffectName = effectName;
		e.Position = pos;

		bus.Publish(e);
	}

	inline void PublishEffectPositionUpdate(EventBus& bus, const std::string& instanceKey, const Math::Vector3& pos)
	{
		EffectPositionUpdate e;
		e.InstanceKey = instanceKey;
		e.Position = pos;

		bus.Publish(e);
	}

	inline void PublishEffectDetach(EventBus& bus, const std::string& instanceKey)
	{
		EffectDetach e;
		e.InstanceKey = instanceKey;

		bus.Publish(e);
	}
}