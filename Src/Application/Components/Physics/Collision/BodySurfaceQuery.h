#pragma once

#include <cassert>
#include <optional>

#include "Application/Components/Physics/Collision/ColliderComponent.h"
#include "Application/Systems/Collision/CollisionMath.h"

// ============================================================
// 胴体形状(ColliderComponent::FindBodyShape)を使った間合いの問い合わせ。
// 状態を持たない純粋な関数だけを置く。
// ============================================================
namespace BodySurfaceQuery
{
	// 胴体のワールドカプセル。未指定/削除済み/Capsule以外ならnullopt。
	inline std::optional<CollisionMath::Capsule> GetBodyCapsule(const ColliderComponent& collider)
	{
		const CollisionShapeEntry* body = collider.FindBodyShape();
		if (body == nullptr) return std::nullopt;

		assert(body->shape == ColliderShape::Capsule);
		if (body->shape != ColliderShape::Capsule) return std::nullopt;

		return collider.GetShapeWorldCapsule(*body);
	}

	// 2者の胴体の水平(XZ)表面間距離。重なっていれば負。取れなければnullopt。
	inline std::optional<float> GetGapXZ(const ColliderComponent& a, const ColliderComponent& b)
	{
		const std::optional<CollisionMath::Capsule> capsuleA = GetBodyCapsule(a);
		const std::optional<CollisionMath::Capsule> capsuleB = GetBodyCapsule(b);
		if (!capsuleA || !capsuleB) return std::nullopt;

		return CollisionMath::CapsuleSurfaceDistanceXZ(*capsuleA, *capsuleB);
	}
}
