#pragma once

#include "Application/Components/Physics/Collision/ColliderComponent.h"
#include "Application/Entity/GameObject.h"

// このフレームに存在する全ColliderComponentの一覧をキャッシュしておく
class ColliderRegistry {
public:
	void Refresh(ObjectManager& objectManager) {
		colliders_ = objectManager.FindComponents<ColliderComponent>();
		objectManager.SetColliderRegistry(this);
	}

	const std::vector<ColliderComponent*>& GetColliders() const { return colliders_; }

private:
	std::vector<ColliderComponent*> colliders_;
};
