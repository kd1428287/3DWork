#pragma once

#include "Event.h"
#include "Application/Definitions/Character/Common/CombatData.h"

class GameObject;
class AttackSourceComponent;

namespace Events
{
	// HPが変化した(ダメージ/回復/SetCurrent/SetMax等)
	struct HealthChanged : public Event {
		Handle<GameObject> source; // 誰のHPが変化したか
		float ratio = 1.0f;        // 変化後の割合(0.0～1.0)
	};

	// HPが0になった(死亡)
	struct Died : public Event {
		Handle<GameObject> source;
	};

	// 体幹値が変化した(被弾/自然回復/リセット時)
	struct PostureChanged : public Event {
		Handle<GameObject> source;
		float ratio = 0.0f;
	};

	struct AttackHit : public Event {
		CollisionMath::OverlapResult hitResult;
		Handle<GameObject> attacker;
		Handle<AttackSourceComponent> attackSource;
		Handle<GameObject> weaponObject;
		AttackDamageData damageData;
		AttackVisualData visualData;
		Math::Vector3 lastValidSwingDir;
	};

	struct ParrySucceeded : public Event
	{
		float parryPostureDamage = 0.0f;
	};

	inline void PublishHealthChangeEvent(EventBus& bus, Handle<GameObject> obj, float ratio)
	{
		HealthChanged e;
		e.source = obj;
		e.ratio = ratio;
		bus.Publish(e);
	}
}