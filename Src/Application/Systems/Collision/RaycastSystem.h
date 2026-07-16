#pragma once

#include "CollisionMath.h"
#include "ColliderRegistry.h"

// ============================================================
// レイキャスト専用のクエリ。
//
// CollisionSystemが「毎フレーム全ペアの重なりを検知してイベントで
// 通知する」常時稼働のSystemなのに対し、こちらは「呼び出し側が
// 好きなタイミングで好きな方向にレイを飛ばして結果を直接受け取る」
// オンデマンドなクエリ。性質が異なるため(常時実行 vs 都度呼び出し、
// イベント通知 vs 戻り値で直接返す)、CollisionSystemとは別クラスに
// 分けている。
//
// KdCollider::Intersects(RayInfo, ...)の役割を引き継ぐが、対象は
// ColliderComponentのSphere/AABB形状のみ(ポリゴン単位の精密な
// レイキャストは未対応)。
//
// 対象コライダーの一覧はColliderRegistryが1フレームに1回キャッシュ
// したものを共有して使う。1フレームに何度も(視界判定、将来的な
// 足IKの接地レイなど、キャラの数だけ)呼ばれる想定のクエリのため、
// 呼ぶたびに全GameObjectを再スキャンするのを避けるための共有。
// (Refresh()した後に増えたコライダーは同フレーム中は見えない。
//  詳細はColliderRegistry参照)
//
// 用途例: 敵の視界判定(ColliderLayer::Sightに向けてレイを飛ばす)、
// 線形の攻撃判定(ColliderLayer::DamageLineに向けてレイを飛ばす)など。
// ============================================================
class RaycastSystem {
public:
	struct Hit {
		GameObject* object = nullptr;
		ColliderComponent* collider = nullptr;
		std::string shapeName;
		CollisionMath::RayHitResult result;
	};

	// rayDirは呼び出し側で正規化しておくこと。
	// layerMaskに一致するレイヤーの形状だけを判定対象にする。
	// 複数当たる場合は距離が最も近い1件だけを返す(貫通しない)。
	// 当たらなければfalseを返す(outHitは変更しない)。
	static bool RaycastClosest(
		const ColliderRegistry& registry,
		const Math::Vector3& origin, const Math::Vector3& direction, float range,
		uint32_t layerMask, Hit& outHit) {

		const std::vector<ColliderComponent*>& colliders = registry.GetColliders();

		bool found = false;
		float closestDistance = range;

		for (ColliderComponent* collider : colliders) {
			if (!IsRaycastable(collider)) continue;

			for (const CollisionShapeEntry& shape : collider->GetShapes()) {
				if (!shape.enabled) continue;
				if ((shape.layer & layerMask) == 0) continue;

				const CollisionMath::RayHitResult result =
					TestShape(collider, shape, origin, direction, range);

				if (!result.hit || result.distance > closestDistance) continue;

				closestDistance = result.distance;
				outHit.object = collider->GetOwner();
				outHit.collider = collider;
				outHit.shapeName = shape.name;
				outHit.result = result;
				found = true;
			}
		}

		return found;
	}

	// 貫通あり: 当たった全ての形状を距離昇順で返す。
	static std::vector<Hit> RaycastAll(
		const ColliderRegistry& registry,
		const Math::Vector3& origin, const Math::Vector3& direction, float range,
		uint32_t layerMask) {

		const std::vector<ColliderComponent*>& colliders = registry.GetColliders();
		std::vector<Hit> hits;

		for (ColliderComponent* collider : colliders) {
			if (!IsRaycastable(collider)) continue;

			for (const CollisionShapeEntry& shape : collider->GetShapes()) {
				if (!shape.enabled) continue;
				if ((shape.layer & layerMask) == 0) continue;

				const CollisionMath::RayHitResult result =
					TestShape(collider, shape, origin, direction, range);

				if (!result.hit) continue;

				Hit hit;
				hit.object = collider->GetOwner();
				hit.collider = collider;
				hit.shapeName = shape.name;
				hit.result = result;
				hits.push_back(std::move(hit));
			}
		}

		std::sort(hits.begin(), hits.end(),
			[](const Hit& a, const Hit& b) { return a.result.distance < b.result.distance; });

		return hits;
	}

private:
	static bool IsRaycastable(ColliderComponent* collider) {
		return collider->IsEnabled() && collider->GetOwner()->IsActive();
	}

	static CollisionMath::RayHitResult TestShape(
		ColliderComponent* collider, const CollisionShapeEntry& shape,
		const Math::Vector3& origin, const Math::Vector3& direction, float range) {

		const Math::Vector3 center = collider->GetShapeWorldCenter(shape);

		return (shape.shape == ColliderShape::Sphere)
			? CollisionMath::RayVsSphere(origin, direction, range, center, shape.radius)
			: CollisionMath::RayVsAABB(origin, direction, range, center, shape.halfExtents);
	}
};