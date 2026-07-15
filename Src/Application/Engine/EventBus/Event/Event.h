#pragma once

class ColliderComponent;

class Event {
public:
	virtual ~Event() = default;
};

namespace Events
{
	namespace Collision
	{
		struct CollisionEnterEvent : public Event
		{
			GameObject* selfObject = nullptr;
			ColliderComponent* selfCollider = nullptr;

			GameObject* otherObject = nullptr;
			ColliderComponent* otherCollider = nullptr;
		};

		struct CollisionExitEvent : public Event
		{
			GameObject* selfObject = nullptr;
			ColliderComponent* selfCollider = nullptr;

			GameObject* otherObject = nullptr;
			ColliderComponent* otherCollider = nullptr;
		};
	}
}