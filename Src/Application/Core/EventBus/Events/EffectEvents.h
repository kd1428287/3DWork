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
		float         MaxDuration = 0.0f; // >0: この秒数が過ぎたら自動でStop(持ち主のDetach漏れ対策)
	};

	// 単発(1回だけ発生)で、発生後は位置更新に合わせて粒子ごと追従するエフェクト。
	// 位置更新はEffectPositionUpdate、途中キャンセルはEffectDetachを使う。
	struct EffectFollowSpawn : public Event
	{
		std::string   InstanceKey;
		std::string   EffectName;
		Math::Vector3 Position;
		float         ConvergeDuration = 0.0f; // >0: Converge層がこの秒数ちょうどで収束しきる
		float         SpawnSpread = 0.0f;      // 0〜1: 粒子の発生を収束時間のこの割合へばらけさせる(0なら最初に全発生)
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

	// 新規発生だけを止め、残っている粒子は自然に消してから破棄する(Detachは即座に全消去)
	struct EffectStop : public Event
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
		const std::string& effectName, const Math::Vector3& pos, float maxDuration = 0.0f)
	{
		EffectAttachSpawn e;
		e.InstanceKey = instanceKey;
		e.EffectName = effectName;
		e.Position = pos;
		e.MaxDuration = maxDuration;

		bus.Publish(e);
	}

	inline void PublishEffectFollowSpawn(EventBus& bus, const std::string& instanceKey,
		const std::string& effectName, const Math::Vector3& pos, float convergeDuration = 0.0f, float spawnSpread = 0.0f)
	{
		EffectFollowSpawn e;
		e.InstanceKey = instanceKey;
		e.EffectName = effectName;
		e.Position = pos;
		e.ConvergeDuration = convergeDuration;
		e.SpawnSpread = spawnSpread;

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

	inline void PublishEffectStop(EventBus& bus, const std::string& instanceKey)
	{
		EffectStop e;
		e.InstanceKey = instanceKey;

		bus.Publish(e);
	}
}