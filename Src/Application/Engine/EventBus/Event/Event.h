#pragma once

#include "../../../Systems/Collision/CollisionMath.h"

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
			std::string selfShapeName;

			GameObject* otherObject = nullptr;
			ColliderComponent* otherCollider = nullptr;
			std::string otherShapeName;

			// selfをotherから押し出す向き・めり込み量など。
			// KdCollider::CollisionResultに相当する詳細情報。
			// hitNormalは常に「self視点」(selfをotherから押し出す向き)になるよう
			// CollisionSystem側で調整済み。
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
		};
	}
}