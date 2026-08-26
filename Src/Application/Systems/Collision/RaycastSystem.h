#pragma once

#include "CollisionMath.h"
#include "../../Components/Collision/ColliderCategory.h"
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
// KdCollider::Intersects(RayInfo, ...)の役割を引き継ぐ。対象は
// ColliderComponentのSphere/Box(OBB)/Capsule/Mesh/Polygonの5種類全て
// (Mesh/PolygonはKdModelCollision/KdPolygonCollisionのvsレイ相当)。
// 以前はMesh/Polygon用に別クラス(TriangleColliderComponent)・
// 別ループを持っていたが、CollisionShapeEntryに統合されたことで
// 1つのループ・1つのHit構造体で済むようになった。
//
// 対象コライダーの一覧はColliderRegistryが1フレームに1回キャッシュ
// したものを共有して使う。1フレームに何度も(視界判定、将来的な
// 足IKの接地レイなど、キャラの数だけ)呼ばれる想定のクエリのため、
// 呼ぶたびに全GameObjectを再スキャンするのを避けるための共有。
// (Refresh()した後に増えたコライダーは同フレーム中は見えない。
//  詳細はColliderRegistry参照)
//
// 用途例: 敵の視界判定(ColliderCategory::Sightに向けてレイを飛ばす)、
// 線形の攻撃判定(ColliderCategory::HitLineに向けてレイを飛ばす)、
// 地形メッシュ(Meshシェイプ)への接地レイなど。
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
// layerMaskに一致するカテゴリの形状だけを判定対象にする。
// excludeOwnerを渡した場合、そのGameObjectが持つ形状は判定対象から
// 除外する(発射点が自分自身のコライダー内部にある場合の自己ヒットを
// 避けるため。例: カメラ用のピボットが追従対象自身のBump
// コライダー内部にあるケース)。省略時はnullptrで従来通り全対象。
// 複数当たる場合は距離が最も近い1件だけを返す(貫通しない)。
// 当たらなければfalseを返す(outHitは変更しない)。
	static bool RaycastClosest(
		const ColliderRegistry& registry,
		const Math::Vector3& origin, const Math::Vector3& direction, float range,
		ColliderCategory layerMask, Hit& outHit,
		GameObject* excludeOwner = nullptr) {

		bool found = false;
		float closestDistance = range;

		for (ColliderComponent* collider : registry.GetColliders()) {
			if (!IsRaycastable(collider)) continue;
			if (excludeOwner != nullptr && collider->GetOwner() == excludeOwner) continue;

			for (const CollisionShapeEntry& shape : collider->GetShapes()) {
				if (!shape.enabled) continue;
				if (!Any(shape.categoryMask & layerMask)) continue;

				const CollisionMath::RayHitResult result =
					TestShape(collider, shape, origin, direction, closestDistance);

				if (!result.hit || result.distance > closestDistance) continue;

				closestDistance = result.distance;
				outHit = Hit{};
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
		ColliderCategory layerMask) {

		std::vector<Hit> hits;

		for (ColliderComponent* collider : registry.GetColliders()) {
			if (!IsRaycastable(collider)) continue;

			for (const CollisionShapeEntry& shape : collider->GetShapes()) {
				if (!shape.enabled) continue;
				if (!Any(shape.categoryMask & layerMask)) continue;

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

	// --- スフィアキャスト(半径を持つ掃引球としての判定) -----------------
//
// 正確な掃引球判定(Minkowski和)が成立するのはSphere/Capsule/Boxの
// 3形状のみ:
//   - Sphere: 半径にsweepRadiusを足すだけで、既存のRayVsSphereが
//     そのまま正確な結果を返す(定義上、厳密に正しい)。
//   - Capsule: 同様に半径を足すだけで厳密に正しい。
//   - Box: 本来「角が丸まった箱」になるはずだが、角の丸め処理は
//     実装せず、halfExtentsをsweepRadius分だけ単純に膨らませた
//     "角が立ったままの箱"で近似する。この近似は常に安全側
//     (実際の丸箱より大きい)に働くため、"稀に貫通を見逃す"方向の
//     誤差にはならない(角付近でカメラが本来よりわずかに早く
//     引き寄せられることはある)。
//
// Mesh/Polygon(地形)は非対応。三角形とのMinkowski和は薄い形状ゆえに
// Boxのような単純な膨らませ近似が使えず(むしろ危険側の誤差になる)、
// 正確な実装には面/辺/頂点の3領域判定が別途必要になる。
// 現状は"半径を考慮しない通常のレイ"にフォールバックしている。
// つまり地形(Mesh/Polygon)については従来通り、細い壁の角などを
// わずかにすり抜ける既知の制約が残ったままである点に注意。
	static bool SphereCastClosest(
		const ColliderRegistry& registry,
		const Math::Vector3& origin, const Math::Vector3& direction, float range,
		float sweepRadius,
		ColliderCategory layerMask, Hit& outHit,
		GameObject* excludeOwner = nullptr) {

		bool found = false;
		float closestDistance = range;

		for (ColliderComponent* collider : registry.GetColliders()) {
			if (!IsRaycastable(collider)) continue;
			if (excludeOwner != nullptr && collider->GetOwner() == excludeOwner) continue;

			for (const CollisionShapeEntry& shape : collider->GetShapes()) {
				if (!shape.enabled) continue;
				if (!Any(shape.categoryMask & layerMask)) continue;

				const CollisionMath::RayHitResult result =
					TestShapeSwept(collider, shape, origin, direction, closestDistance, sweepRadius);

				if (!result.hit || result.distance > closestDistance) continue;

				closestDistance = result.distance;
				outHit = Hit{};
				outHit.object = collider->GetOwner();
				outHit.collider = collider;
				outHit.shapeName = shape.name;
				outHit.result = result;
				found = true;
			}
		}

		return found;
	}

private:

	static CollisionMath::RayHitResult TestShapeSwept(
		ColliderComponent* collider, const CollisionShapeEntry& shape,
		const Math::Vector3& origin, const Math::Vector3& direction, float range,
		float sweepRadius) {

		if (shape.shape == ColliderShape::Sphere) {
			const Math::Vector3 center = collider->GetShapeWorldCenter(shape);
			const float radius = collider->GetShapeWorldRadius(shape) + sweepRadius;
			return CollisionMath::RayVsSphere(origin, direction, range, center, radius);
		}

		if (shape.shape == ColliderShape::Box) {
			CollisionMath::OrientedBox box = collider->GetShapeWorldOBB(shape);
			box.halfExtents += Math::Vector3(sweepRadius, sweepRadius, sweepRadius); // 角が立ったままの安全側近似
			return CollisionMath::RayVsOBB(origin, direction, range, box);
		}

		if (shape.shape == ColliderShape::Capsule) {
			CollisionMath::Capsule capsule = collider->GetShapeWorldCapsule(shape);
			capsule.radius += sweepRadius;
			return CollisionMath::RayVsCapsule(origin, direction, range, capsule);
		}

		// Mesh/Polygon: 上記コメントの通り未対応。半径を考慮しない
		// 通常のレイにフォールバックする。
		return shape.TestTriangleVsRay(collider->GetWorldMatrix(), origin, direction, range);
	}

	static bool IsRaycastable(ColliderComponent* collider) {
		return collider->IsEnabled() && collider->GetOwner()->IsActive();
	}

	static CollisionMath::RayHitResult TestShape(
		ColliderComponent* collider, const CollisionShapeEntry& shape,
		const Math::Vector3& origin, const Math::Vector3& direction, float range) {

		if (shape.shape == ColliderShape::Sphere) {
			const Math::Vector3 center = collider->GetShapeWorldCenter(shape);
			const float radius = collider->GetShapeWorldRadius(shape);
			return CollisionMath::RayVsSphere(origin, direction, range, center, radius);
		}

		if (shape.shape == ColliderShape::Box) {
			const CollisionMath::OrientedBox box = collider->GetShapeWorldOBB(shape);
			return CollisionMath::RayVsOBB(origin, direction, range, box);
		}

		if (shape.shape == ColliderShape::Capsule) {
			return CollisionMath::RayVsCapsule(origin, direction, range, collider->GetShapeWorldCapsule(shape));
		}

		// Mesh/Polygon: ワールド行列は形状エントリ自身のキャッシュに委ねる。
		return shape.TestTriangleVsRay(collider->GetWorldMatrix(), origin, direction, range);
	}
};