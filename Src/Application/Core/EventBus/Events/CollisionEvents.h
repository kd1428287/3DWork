#pragma once

#include "Event.h"
#include <string>

class GameObject;
class ColliderComponent;

namespace Events
{
	struct CollisionEnterEvent : public Event
	{
		GameObject* selfObject = nullptr;
		ColliderComponent* selfCollider = nullptr;
		std::string selfShapeName;

		GameObject* otherObject = nullptr;
		ColliderComponent* otherCollider = nullptr;
		std::string otherShapeName;

		ColliderCategory selfCategory;
		ColliderCategory otherCategory;

		bool SelfIs(ColliderCategory c) const { return Any(selfCategory & c); }
		bool OtherIs(ColliderCategory c) const { return Any(otherCategory & c); }

		CollisionMath::OverlapResult hitResult;
	};

	struct CollisionStayEvent : public Event
	{
		GameObject* selfObject = nullptr;
		ColliderComponent* selfCollider = nullptr;
		std::string selfShapeName;

		GameObject* otherObject = nullptr;
		ColliderComponent* otherCollider = nullptr;
		std::string otherShapeName;

		ColliderCategory selfCategory;
		ColliderCategory otherCategory;

		bool SelfIs(ColliderCategory c) const { return Any(selfCategory & c); }
		bool OtherIs(ColliderCategory c) const { return Any(otherCategory & c); }

		CollisionMath::OverlapResult hitResult;
	};

	struct CollisionExitEvent : public Event
	{
		GameObject* selfObject = nullptr;
		ColliderComponent* selfCollider = nullptr;
		std::string selfShapeName;

		GameObject* otherObject = nullptr;
		ColliderComponent* otherCollider = nullptr;
		std::string otherShapeName;

		ColliderCategory selfCategory;
		ColliderCategory otherCategory;

		bool SelfIs(ColliderCategory c) const { return Any(selfCategory & c); }
		bool OtherIs(ColliderCategory c) const { return Any(otherCategory & c); }
	};
}