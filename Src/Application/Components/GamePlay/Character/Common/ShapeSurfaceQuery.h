#pragma once

#include <algorithm>
#include <optional>

#include "Application/Components/Physics/Collision/ColliderComponent.h"
#include "Application/Systems/Collision/CollisionMath.h"

// ============================================================
// 形状(Sphere/Box/Capsule)への最近点問い合わせ。
// 状態を持たない純粋な関数だけを置く(BodySurfaceQuery同様)。
// ============================================================
namespace ShapeSurfaceQuery
{
	struct ClosestPair
	{
		Math::Vector3 onA; // A形状上の点
		Math::Vector3 onB; // B形状上の点
	};

	// 形状の代表点(Capsuleは軸の中点)。
	inline Math::Vector3 GetShapeCenter(const ColliderComponent& collider, const CollisionShapeEntry& entry)
	{
		if (entry.shape == ColliderShape::Capsule) {
			const CollisionMath::Capsule c = collider.GetShapeWorldCapsule(entry);
			return (c.start + c.end) * 0.5f;
		}
		return collider.GetShapeWorldCenter(entry);
	}

	// 点pに最も近い形状上の点(中身を含む。pが内部ならp自身)。
	// Sphere/Box/Capsule以外はnullopt。
	inline std::optional<Math::Vector3> ClosestPointOnShape(
		const ColliderComponent& collider, const CollisionShapeEntry& entry, const Math::Vector3& p)
	{
		switch (entry.shape) {
		case ColliderShape::Sphere:
		{
			const Math::Vector3 center = collider.GetShapeWorldCenter(entry);
			const float radius = collider.GetShapeWorldRadius(entry);
			const Math::Vector3 d = p - center;
			const float len = d.Length();
			if (len <= radius) return p;
			return center + d * (radius / len);
		}
		case ColliderShape::Box:
		{
			const CollisionMath::OrientedBox box = collider.GetShapeWorldOBB(entry);
			Math::Quaternion invRot;
			box.orientation.Conjugate(invRot);

			// ボックスのローカル空間でクランプして戻す。
			Math::Vector3 local = Math::Vector3::Transform(p - box.center, invRot);
			local.x = std::clamp(local.x, -box.halfExtents.x, box.halfExtents.x);
			local.y = std::clamp(local.y, -box.halfExtents.y, box.halfExtents.y);
			local.z = std::clamp(local.z, -box.halfExtents.z, box.halfExtents.z);
			return Math::Vector3::Transform(local, box.orientation) + box.center;
		}
		case ColliderShape::Capsule:
		{
			const CollisionMath::Capsule capsule = collider.GetShapeWorldCapsule(entry);
			const Math::Vector3 axisPoint = CollisionMath::ClosestPointOnSegment(p, capsule.start, capsule.end);
			const Math::Vector3 d = p - axisPoint;
			const float len = d.Length();
			if (len <= capsule.radius) return p;
			return axisPoint + d * (capsule.radius / len);
		}
		default:
			return std::nullopt;
		}
	}

	// 2形状間の最近点対(凸形状前提)。交互に射影する近似で、数回で十分収束する。
	// 重なっている場合は両点が一致(または交差領域内の点)になる。
	inline std::optional<ClosestPair> ClosestPointsBetweenShapes(
		const ColliderComponent& colliderA, const CollisionShapeEntry& a,
		const ColliderComponent& colliderB, const CollisionShapeEntry& b,
		int iterations = 3)
	{
		Math::Vector3 pointB = GetShapeCenter(colliderB, b);
		Math::Vector3 pointA = pointB;

		for (int i = 0; i < iterations; ++i) {
			const std::optional<Math::Vector3> nextA = ClosestPointOnShape(colliderA, a, pointB);
			if (!nextA) return std::nullopt;
			pointA = *nextA;

			const std::optional<Math::Vector3> nextB = ClosestPointOnShape(colliderB, b, pointA);
			if (!nextB) return std::nullopt;
			pointB = *nextB;
		}
		return ClosestPair{ pointA, pointB };
	}
}
