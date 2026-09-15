#pragma once

#include "Application/Core/EventBus/Events/CollisionEvents.h"
#include "Application/Components/Physics/Collision/ColliderComponent.h"

#include "CollisionMath.h"

// 当たり判定イベントの生成と配信だけを担当するクラス
// 宛先は各GameObjectのローカルバス
class CollisionEventPublisher {
public:
	using CollisionEnterEvent = Events::Collision::CollisionEnterEvent;
	using CollisionExitEvent = Events::Collision::CollisionExitEvent;
	using CollisionStayEvent = Events::Collision::CollisionStayEvent;

	static void PublishEnter(
		ColliderComponent* a, const CollisionShapeEntry& shapeA,
		ColliderComponent* b, const CollisionShapeEntry& shapeB,
		const CollisionMath::OverlapResult& overlap) {

		PublishToOwner(a, MakeEnterEvent(a, shapeA, b, shapeB, overlap, false));
		PublishToOwner(b, MakeEnterEvent(b, shapeB, a, shapeA, overlap, true));
	}

	static void PublishExit(
		ColliderComponent* a, const CollisionShapeEntry& shapeA,
		ColliderComponent* b, const CollisionShapeEntry& shapeB) {

		PublishToOwner(a, MakeExitEvent(a, shapeA, b, shapeB));
		PublishToOwner(b, MakeExitEvent(b, shapeB, a, shapeA));
	}

	// wantsStayA/wantsStayBは呼び出し側(CollisionSystem::Update())が
	// 「shapeAだけ欲しい」「shapeBだけ欲しい」というケースを区別済みで渡す。
	// 片側だけ発行することを許すために個別に受け取る。
	static void PublishStay(
		ColliderComponent* a, const CollisionShapeEntry& shapeA,
		ColliderComponent* b, const CollisionShapeEntry& shapeB,
		const CollisionMath::OverlapResult& overlap,
		bool wantsStayA, bool wantsStayB) {

		if (wantsStayA) {
			PublishToOwner(a, MakeStayEvent(a, shapeA, b, shapeB, overlap, false));
		}
		if (wantsStayB) {
			PublishToOwner(b, MakeStayEvent(b, shapeB, a, shapeA, overlap, true));
		}
	}

private:
	static CollisionEnterEvent MakeEnterEvent(
		ColliderComponent* self, const CollisionShapeEntry& selfShape,
		ColliderComponent* other, const CollisionShapeEntry& otherShape,
		const CollisionMath::OverlapResult& hitResult, bool flipNormal) {

		CollisionEnterEvent e;
		e.selfObject = self->GetOwner();
		e.selfCollider = self;
		e.selfShapeName = selfShape.name;
		e.otherObject = other->GetOwner();
		e.otherCollider = other;
		e.otherShapeName = otherShape.name;
		e.hitResult = hitResult;
		if (flipNormal) {
			e.hitResult.hitNormal = -e.hitResult.hitNormal;
		}
		return e;
	}

	static CollisionExitEvent MakeExitEvent(
		ColliderComponent* self, const CollisionShapeEntry& selfShape,
		ColliderComponent* other, const CollisionShapeEntry& otherShape) {

		CollisionExitEvent e;
		e.selfObject = self->GetOwner();
		e.selfCollider = self;
		e.selfShapeName = selfShape.name;
		e.otherObject = other->GetOwner();
		e.otherCollider = other;
		e.otherShapeName = otherShape.name;
		return e;
	}

	static CollisionStayEvent MakeStayEvent(
		ColliderComponent* self, const CollisionShapeEntry& selfShape,
		ColliderComponent* other, const CollisionShapeEntry& otherShape,
		const CollisionMath::OverlapResult& hitResult, bool flipNormal) {

		// フィールド構成はCollisionEnterEventと同一。型を分けているのは
		// 購読側がEnter/Stayを別々に選べるようにするため。
		CollisionStayEvent e;
		e.selfObject = self->GetOwner();
		e.selfCollider = self;
		e.selfShapeName = selfShape.name;
		e.otherObject = other->GetOwner();
		e.otherCollider = other;
		e.otherShapeName = otherShape.name;
		e.hitResult = hitResult;
		if (flipNormal) {
			e.hitResult.hitNormal = -e.hitResult.hitNormal;
		}
		return e;
	}

	static void PublishToOwner(ColliderComponent* self, auto&& event) {
		self->GetOwner()->GetLocalEventBus().Publish(std::forward<decltype(event)>(event));
	}
};
