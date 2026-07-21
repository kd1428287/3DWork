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

		struct CollisionStayEvent : Event {
			GameObject* selfObject = nullptr;
			ColliderComponent* selfCollider = nullptr;
			std::string selfShapeName;
			GameObject* otherObject = nullptr;
			ColliderComponent* otherCollider = nullptr;
			std::string otherShapeName;
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

		// 以前はMesh/PolygonColliderComponent(TriangleColliderComponent)との
		// 接触通知用にTerrainCollisionEnterEvent/TerrainCollisionExitEventを
		// 別途持っていたが、Mesh/PolygonがColliderComponentの
		// CollisionShapeEntryへ統合されたことで、相手も常にColliderComponent
		// になったため不要になった(CollisionEnterEvent/CollisionExitEventで
		// Mesh/Polygonとの接触も含めて表現できる)。
	}
};
