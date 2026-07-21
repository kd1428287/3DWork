#pragma once

// ============================================================
// 当たり判定の純粋な幾何計算だけを集めた場所。
//
// KdCollision.cpp(ポリゴン/メッシュ含む低レベル判定関数群)の役割を
// 引き継ぐ部分。Sphere/Box(OBB。回転を考慮するBOX)/Rayの基本形状に
// 加え、三角形単位の判定(KdPointToTriangle/DirectX::TriangleTests::
// Intersects相当)もここに集約している。
//
// 三角形1枚に対する判定はここでは「1枚だけ」を見て結果を返す関数と
// している。複数の三角形(メッシュ/ポリゴン)をまとめて相手にする際の
// 反復処理(境界での足切り、複数面にまたがる押し出しの合成、ワールド
// 変換のキャッシュなど)はColliderComponent::CollisionShapeEntry側の
// 責務にしている(TestTriangleVsSphere/TestTriangleVsOBB/
// TestTriangleVsRay参照)。KdModelCollision/KdPolygonCollisionが
// 「全ての面をループしながら結果を合成する」役目を持っていたのと同じ
// 責務分担を、コンポーネント指向向けに引き継いだもの。
//
// GameObjectやComponentを一切知らない、状態を持たない純粋関数の
// 集まりにしている。ColliderComponent/CollisionSystem/RaycastSystemの
// どこからでも同じ計算を使い回せるようにするため。
// ============================================================
namespace CollisionMath
{
	// 形状同士の重なり判定結果。KdCollider::CollisionResultの役割を
	// 引き継ぐが、名前はこのプロジェクトの命名に合わせている。
	struct OverlapResult
	{
		bool hit = false;
		Math::Vector3 hitPos;          // 代表的な衝突点
		Math::Vector3 hitNormal;       // 「a側」を「b側」から押し出す向き(正規化済み)
		float overlapDistance = 0.0f;  // めり込み量
	};

	// レイ判定結果。
	struct RayHitResult
	{
		bool hit = false;
		Math::Vector3 hitPos;
		Math::Vector3 hitNormal;
		float distance = 0.0f; // レイの発射点からの距離
	};

	inline OverlapResult SphereVsSphere(
		const Math::Vector3& centerA, float radiusA,
		const Math::Vector3& centerB, float radiusB)
	{
		OverlapResult result;

		const Math::Vector3 diff = centerA - centerB;
		const float distSq = diff.LengthSquared();
		const float radiusSum = radiusA + radiusB;

		if (distSq > radiusSum * radiusSum) return result; // 重なっていない

		const float dist = std::sqrt(distSq);
		result.hit = true;
		result.overlapDistance = radiusSum - dist;

		if (dist > 1e-6f) {
			result.hitNormal = diff / dist;
		}
		else {
			// 中心が完全一致(理論上稀)。押し出し方向を決められないので
			// 便宜的に上方向にしておく。
			result.hitNormal = Math::Vector3::Up;
		}

		result.hitPos = centerB + result.hitNormal * radiusB;
		return result;
	}

	inline OverlapResult AABBVsAABB(
		const Math::Vector3& centerA, const Math::Vector3& halfA,
		const Math::Vector3& centerB, const Math::Vector3& halfB)
	{
		OverlapResult result;

		const Math::Vector3 diff = centerA - centerB;
		const float overlapX = (halfA.x + halfB.x) - std::abs(diff.x);
		const float overlapY = (halfA.y + halfB.y) - std::abs(diff.y);
		const float overlapZ = (halfA.z + halfB.z) - std::abs(diff.z);

		if (overlapX <= 0.0f || overlapY <= 0.0f || overlapZ <= 0.0f) return result;

		result.hit = true;

		// めり込みが最も小さい軸を押し出し方向として選ぶ
		// (最小移動量で分離できる軸、という一般的なAABB分離手法)。
		if (overlapX <= overlapY && overlapX <= overlapZ) {
			result.overlapDistance = overlapX;
			result.hitNormal = Math::Vector3(diff.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
		}
		else if (overlapY <= overlapX && overlapY <= overlapZ) {
			result.overlapDistance = overlapY;
			result.hitNormal = Math::Vector3(0.0f, diff.y >= 0.0f ? 1.0f : -1.0f, 0.0f);
		}
		else {
			result.overlapDistance = overlapZ;
			result.hitNormal = Math::Vector3(0.0f, 0.0f, diff.z >= 0.0f ? 1.0f : -1.0f);
		}

		// 正確な接触点までは求めず、押し出し軸に沿った簡易的な近似点にする
		result.hitPos = centerA - result.hitNormal * (result.overlapDistance * 0.5f);
		return result;
	}

	// sphere側から見た結果(hitNormalはsphereをaabbから押し出す向き)を返す。
	inline OverlapResult SphereVsAABB(
		const Math::Vector3& sphereCenter, float sphereRadius,
		const Math::Vector3& boxCenter, const Math::Vector3& boxHalf)
	{
		OverlapResult result;

		const Math::Vector3 closest{
			std::clamp(sphereCenter.x, boxCenter.x - boxHalf.x, boxCenter.x + boxHalf.x),
			std::clamp(sphereCenter.y, boxCenter.y - boxHalf.y, boxCenter.y + boxHalf.y),
			std::clamp(sphereCenter.z, boxCenter.z - boxHalf.z, boxCenter.z + boxHalf.z),
		};

		const Math::Vector3 diff = sphereCenter - closest;
		const float distSq = diff.LengthSquared();

		if (distSq > sphereRadius * sphereRadius) return result;

		result.hit = true;
		const float dist = std::sqrt(distSq);

		if (dist > 1e-6f) {
			result.hitNormal = diff / dist;
			result.overlapDistance = sphereRadius - dist;
		}
		else {
			// 球の中心がAABBの内部にある(深く貫通済み)。最も浅い面へ
			// 押し出す簡易処理にする(AABBVsAABBと同じ最小軸選択の考え方)。
			const Math::Vector3 toMax = (boxCenter + boxHalf) - sphereCenter;
			const Math::Vector3 toMin = sphereCenter - (boxCenter - boxHalf);

			float minPenetration = toMax.x; result.hitNormal = Math::Vector3(1, 0, 0);
			if (toMin.x < minPenetration) { minPenetration = toMin.x; result.hitNormal = Math::Vector3(-1, 0, 0); }
			if (toMax.y < minPenetration) { minPenetration = toMax.y; result.hitNormal = Math::Vector3(0, 1, 0); }
			if (toMin.y < minPenetration) { minPenetration = toMin.y; result.hitNormal = Math::Vector3(0, -1, 0); }
			if (toMax.z < minPenetration) { minPenetration = toMax.z; result.hitNormal = Math::Vector3(0, 0, 1); }
			if (toMin.z < minPenetration) { minPenetration = toMin.z; result.hitNormal = Math::Vector3(0, 0, -1); }

			result.overlapDistance = sphereRadius + minPenetration;
		}

		result.hitPos = sphereCenter - result.hitNormal * sphereRadius;
		return result;
	}

	// rayDirは正規化済みである前提(呼び出し側で正規化しておくこと)。
	inline RayHitResult RayVsSphere(
		const Math::Vector3& rayOrigin, const Math::Vector3& rayDir, float rayRange,
		const Math::Vector3& sphereCenter, float sphereRadius)
	{
		RayHitResult result;

		const Math::Vector3 originToCenter = sphereCenter - rayOrigin;
		const float projLength = originToCenter.Dot(rayDir);

		// 球が完全にレイの後方にあるなら明らかに当たらない
		if (projLength < -sphereRadius) return result;

		const float distToCenterSq = originToCenter.LengthSquared();
		const float halfChordSq = sphereRadius * sphereRadius - (distToCenterSq - projLength * projLength);
		if (halfChordSq < 0.0f) return result; // レイが球の外側を通っている

		const float distance = projLength - std::sqrt(halfChordSq);
		if (distance < 0.0f || distance > rayRange) return result;

		result.hit = true;
		result.distance = distance;
		result.hitPos = rayOrigin + rayDir * distance;
		result.hitNormal = result.hitPos - sphereCenter;
		result.hitNormal.Normalize();
		return result;
	}

	// スラブ法によるレイ vs AABB判定。
	// ※ レイの発射点が既にAABB内部から始まっているケースは
	//   「当たらない」扱いにする簡易実装(前進方向の脱出交点は求めない)。
	inline RayHitResult RayVsAABB(
		const Math::Vector3& rayOrigin, const Math::Vector3& rayDir, float rayRange,
		const Math::Vector3& boxCenter, const Math::Vector3& boxHalf)
	{
		RayHitResult result;

		const Math::Vector3 boxMin = boxCenter - boxHalf;
		const Math::Vector3 boxMax = boxCenter + boxHalf;

		float tMin = 0.0f;
		float tMax = rayRange;
		int hitAxis = -1;
		float hitSign = 1.0f;

		const float dirs[3] = { rayDir.x, rayDir.y, rayDir.z };
		const float origins[3] = { rayOrigin.x, rayOrigin.y, rayOrigin.z };
		const float mins[3] = { boxMin.x, boxMin.y, boxMin.z };
		const float maxs[3] = { boxMax.x, boxMax.y, boxMax.z };

		for (int axis = 0; axis < 3; ++axis) {
			if (std::abs(dirs[axis]) < 1e-6f) {
				// この軸方向にはレイが進まない。原点がスラブの外なら絶対当たらない
				if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) return result;
				continue;
			}

			float t1 = (mins[axis] - origins[axis]) / dirs[axis];
			float t2 = (maxs[axis] - origins[axis]) / dirs[axis];
			float sign = -1.0f;
			if (t1 > t2) { std::swap(t1, t2); sign = 1.0f; }

			if (t1 > tMin) { tMin = t1; hitAxis = axis; hitSign = sign; }
			tMax = std::min(tMax, t2);

			if (tMin > tMax) return result;
		}

		if (hitAxis == -1) {
			// 原点自体がボックス内部から始まっている特殊ケース。
			// 今回は「当たらない」扱いにしておく(呼び出し側で別途考慮すること)。
			return result;
		}

		result.hit = true;
		result.distance = tMin;
		result.hitPos = rayOrigin + rayDir * tMin;
		result.hitNormal = Math::Vector3::Zero;
		if (hitAxis == 0) result.hitNormal.x = hitSign;
		else if (hitAxis == 1) result.hitNormal.y = hitSign;
		else result.hitNormal.z = hitSign;

		return result;
	}

	// ------------------------------------------------------------
	// 三角形単位の判定
	// KdCollision.cpp(KdPointToTriangle/DirectX::TriangleTests::Intersects
	// を使っていた部分)の役割を引き継ぐ。ここでは三角形1枚のみを見る。
	// ------------------------------------------------------------

	// 三角形の表向き法線(v0->v1->v2の順を表とする)。
	inline Math::Vector3 TriangleNormal(
		const Math::Vector3& v0, const Math::Vector3& v1, const Math::Vector3& v2)
	{
		Math::Vector3 normal = (v1 - v0).Cross(v2 - v0);
		normal.Normalize();
		return normal;
	}

	// 点から三角形への最近接点を求める。
	// ※出典: 「リアルタイム衝突判定」(Christer Ericson) の重心座標法。
	//   KdPointToTriangleと同一のアルゴリズム。
	inline Math::Vector3 ClosestPointOnTriangle(
		const Math::Vector3& p,
		const Math::Vector3& a, const Math::Vector3& b, const Math::Vector3& c)
	{
		const Math::Vector3 ab = b - a;
		const Math::Vector3 ac = c - a;
		const Math::Vector3 ap = p - a;

		const float d1 = ab.Dot(ap);
		const float d2 = ac.Dot(ap);
		if (d1 <= 0.0f && d2 <= 0.0f) return a; // 重心座標(1,0,0)

		const Math::Vector3 bp = p - b;
		const float d3 = ab.Dot(bp);
		const float d4 = ac.Dot(bp);
		if (d3 >= 0.0f && d4 <= d3) return b; // 重心座標(0,1,0)

		const float vc = d1 * d4 - d3 * d2;
		if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
			const float v = d1 / (d1 - d3);
			return a + ab * v; // 辺ab上
		}

		const Math::Vector3 cp = p - c;
		const float d5 = ab.Dot(cp);
		const float d6 = ac.Dot(cp);
		if (d6 >= 0.0f && d5 <= d6) return c; // 重心座標(0,0,1)

		const float vb = d5 * d2 - d1 * d6;
		if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
			const float w = d2 / (d2 - d6);
			return a + ac * w; // 辺ac上
		}

		const float va = d3 * d6 - d5 * d4;
		if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
			const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
			return b + (c - b) * w; // 辺bc上
		}

		// 三角形の内部
		const float denom = 1.0f / (va + vb + vc);
		const float v = vb * denom;
		const float w = vc * denom;
		return a + ab * v + ac * w;
	}

	// 球 vs 三角形1枚。closest point法による判定
	// (KdCollision.cppのKdPointToTriangle+HitCheckAndPosUpdate相当)。
	// hitNormalは「球を三角形から押し出す向き」。
	inline OverlapResult SphereVsTriangle(
		const Math::Vector3& sphereCenter, float sphereRadius,
		const Math::Vector3& v0, const Math::Vector3& v1, const Math::Vector3& v2)
	{
		OverlapResult result;

		const Math::Vector3 nearest = ClosestPointOnTriangle(sphereCenter, v0, v1, v2);
		const Math::Vector3 diff = sphereCenter - nearest;
		const float distSq = diff.LengthSquared();

		if (distSq > sphereRadius * sphereRadius) return result;

		result.hit = true;
		const float dist = std::sqrt(distSq);

		if (dist > 1e-6f) {
			result.hitNormal = diff / dist;
		}
		else {
			// 球の中心が面上(境界)にある。押し出し方向として面の法線を使う。
			result.hitNormal = TriangleNormal(v0, v1, v2);
		}

		result.overlapDistance = sphereRadius - dist;
		result.hitPos = nearest;
		return result;
	}

	// AABB vs 三角形1枚。
	// 正確な交差判定にはSAT(分離軸判定/13軸)を使うが、押し出し量・向きの
	// 計算は「三角形の平面法線方向にBOXを投影して押し出す」近似で行う
	// (KdCollider/KdCollision.cppには元々BOX-vs-メッシュの実装が無く
	//  TODOだった部分。地形メッシュに箱がめり込むケースを扱うために
	//  今回新規に追加している)。
	// ※非常に鋭い角にBOXの角が刺さるようなケースでは、この近似だと
	//   押し出し方向がわずかに不自然になることがある(面の広い場所での
	//   押し出しを優先する設計のため、地形のような用途では実用上問題ない)。
	inline OverlapResult AABBVsTriangle(
		const Math::Vector3& boxCenter, const Math::Vector3& boxHalf,
		const Math::Vector3& v0, const Math::Vector3& v1, const Math::Vector3& v2)
	{
		OverlapResult result;

		// ----- SAT: 三角形のエッジ×BOX主軸(9軸) + BOX主軸(3軸) + 三角形法線(1軸) -----
		const Math::Vector3 boxAxes[3] = { Math::Vector3(1,0,0), Math::Vector3(0,1,0), Math::Vector3(0,0,1) };

		const Math::Vector3 t0 = v0 - boxCenter;
		const Math::Vector3 t1 = v1 - boxCenter;
		const Math::Vector3 t2 = v2 - boxCenter;

		auto overlapsOnAxis = [&](const Math::Vector3& axis) -> bool
		{
			const float axisLenSq = axis.LengthSquared();
			if (axisLenSq < 1e-10f) return true; // 縮退した軸は判定不能なので無視(重なり扱い)

			const float p0 = axis.Dot(t0);
			const float p1 = axis.Dot(t1);
			const float p2 = axis.Dot(t2);
			const float triMin = std::min({ p0, p1, p2 });
			const float triMax = std::max({ p0, p1, p2 });

			const float boxRadius =
				boxHalf.x * std::abs(axis.x) + boxHalf.y * std::abs(axis.y) + boxHalf.z * std::abs(axis.z);

			return !(triMin > boxRadius || triMax < -boxRadius);
		};

		// BOX主軸
		for (const Math::Vector3& axis : boxAxes) {
			if (!overlapsOnAxis(axis)) return result;
		}

		// 三角形のエッジ×BOX主軸
		const Math::Vector3 edges[3] = { t1 - t0, t2 - t1, t0 - t2 };
		for (const Math::Vector3& edge : edges) {
			for (const Math::Vector3& boxAxis : boxAxes) {
				if (!overlapsOnAxis(edge.Cross(boxAxis))) return result;
			}
		}

		// 三角形の法線
		const Math::Vector3 normal = TriangleNormal(v0, v1, v2);
		if (!overlapsOnAxis(normal)) return result;

		// ----- ここまで来たら交差確定。押し出しは法線方向への投影で近似する -----
		const float planeDist = normal.Dot(boxCenter - v0); // BOX中心から三角形の面までの符号付き距離
		const float boxRadiusOnNormal =
			boxHalf.x * std::abs(normal.x) + boxHalf.y * std::abs(normal.y) + boxHalf.z * std::abs(normal.z);

		result.hit = true;
		result.hitNormal = (planeDist >= 0.0f) ? normal : -normal;
		result.overlapDistance = boxRadiusOnNormal - std::abs(planeDist);
		result.hitPos = boxCenter - result.hitNormal * boxRadiusOnNormal;
		return result;
	}

	// rayDirは正規化済みである前提。Möller-Trumboreアルゴリズムによる
	// レイ vs 三角形1枚の判定(DirectX::TriangleTests::Intersects相当)。
	// ※両面にヒットする(裏面からでも貫通しない)。片面カリングが
	//   必要な用途(視界を遮る壁など、裏側からは素通りしてほしい場合)は
	//   呼び出し側でdet(=法線とレイの向きの関係)を見て弾くこと。
	inline RayHitResult RayVsTriangle(
		const Math::Vector3& rayOrigin, const Math::Vector3& rayDir, float rayRange,
		const Math::Vector3& v0, const Math::Vector3& v1, const Math::Vector3& v2)
	{
		RayHitResult result;

		const Math::Vector3 edge1 = v1 - v0;
		const Math::Vector3 edge2 = v2 - v0;
		const Math::Vector3 pvec = rayDir.Cross(edge2);
		const float det = edge1.Dot(pvec);

		constexpr float kEpsilon = 1e-6f;
		if (det > -kEpsilon && det < kEpsilon) return result; // レイと三角形が平行

		const float invDet = 1.0f / det;
		const Math::Vector3 tvec = rayOrigin - v0;

		const float u = tvec.Dot(pvec) * invDet;
		if (u < 0.0f || u > 1.0f) return result;

		const Math::Vector3 qvec = tvec.Cross(edge1);
		const float v = rayDir.Dot(qvec) * invDet;
		if (v < 0.0f || u + v > 1.0f) return result;

		const float dist = edge2.Dot(qvec) * invDet;
		if (dist < 0.0f || dist > rayRange) return result;

		result.hit = true;
		result.distance = dist;
		result.hitPos = rayOrigin + rayDir * dist;
		result.hitNormal = TriangleNormal(v0, v1, v2);
		return result;
	}

	// ============================================================
	// OBB(回転を考慮するBOX)の判定。
	//
	// KdCollider.hのDirectX::BoundingOrientedBox相当だが、DirectXの型に
	// 依存せず Vector3(中心) + Vector3(半径サイズ) + Quaternion(向き) の
	// 組で表す(このプロジェクトの他の型と同じくMath::Vector3/Quaternionで
	// 完結させるため)。
	//
	// 実装方針:「一方のBOXのローカル座標系(回転を打ち消した空間)に
	// 相手を持ち込んでから、既存のAABB用の関数を再利用する」という
	// 手法を基本にしている。
	//   - 球・レイ相手: 球もレイも回転に依存しない形状なので、BOXの
	//     ローカル空間に持ち込めばそのままAABBVsシリーズが使える。
	//   - 三角形相手: 三角形の3頂点をBOXのローカル空間に持ち込めば、
	//     やはりAABBVsTriangle(SAT)がそのまま使える。
	//   - OBB相手(BOX同士): 相手も回転しているため、片方をローカル空間に
	//     持ち込むだけでは軸並行にならない。この組み合わせだけは
	//     15軸(両BOXの主軸3+3、主軸同士の外積9)のSATを別途実装する。
	// ============================================================
	struct OrientedBox
	{
		Math::Vector3		center{};
		Math::Vector3		halfExtents{ 0.5f, 0.5f, 0.5f };
		Math::Quaternion	orientation = Math::Quaternion::Identity;
	};

	// 球 vs OBB。
	inline OverlapResult SphereVsOBB(
		const Math::Vector3& sphereCenter, float sphereRadius, const OrientedBox& box)
	{
		Math::Quaternion invRot;
		box.orientation.Conjugate(invRot);

		// BOXのローカル空間(中心が原点、回転なし)へ持ち込む
		const Math::Vector3 localCenter = Math::Vector3::Transform(sphereCenter - box.center, invRot);

		OverlapResult local = SphereVsAABB(localCenter, sphereRadius, Math::Vector3::Zero, box.halfExtents);
		if (!local.hit) return local;

		// ワールド空間へ戻す
		OverlapResult result;
		result.hit = true;
		result.hitNormal = Math::Vector3::Transform(local.hitNormal, box.orientation);
		result.overlapDistance = local.overlapDistance;
		result.hitPos = Math::Vector3::Transform(local.hitPos, box.orientation) + box.center;
		return result;
	}

	// レイ vs OBB。
	inline RayHitResult RayVsOBB(
		const Math::Vector3& rayOrigin, const Math::Vector3& rayDir, float rayRange, const OrientedBox& box)
	{
		Math::Quaternion invRot;
		box.orientation.Conjugate(invRot);

		const Math::Vector3 localOrigin = Math::Vector3::Transform(rayOrigin - box.center, invRot);
		const Math::Vector3 localDir = Math::Vector3::Transform(rayDir, invRot); // 方向ベクトルなので平行移動しない

		RayHitResult local = RayVsAABB(localOrigin, localDir, rayRange, Math::Vector3::Zero, box.halfExtents);
		if (!local.hit) return local;

		RayHitResult result;
		result.hit = true;
		result.distance = local.distance;
		result.hitPos = Math::Vector3::Transform(local.hitPos, box.orientation) + box.center;
		result.hitNormal = Math::Vector3::Transform(local.hitNormal, box.orientation);
		return result;
	}

	// 三角形1枚 vs OBB。
	inline OverlapResult OBBVsTriangle(
		const OrientedBox& box, const Math::Vector3& v0, const Math::Vector3& v1, const Math::Vector3& v2)
	{
		Math::Quaternion invRot;
		box.orientation.Conjugate(invRot);

		const Math::Vector3 lv0 = Math::Vector3::Transform(v0 - box.center, invRot);
		const Math::Vector3 lv1 = Math::Vector3::Transform(v1 - box.center, invRot);
		const Math::Vector3 lv2 = Math::Vector3::Transform(v2 - box.center, invRot);

		OverlapResult local = AABBVsTriangle(Math::Vector3::Zero, box.halfExtents, lv0, lv1, lv2);
		if (!local.hit) return local;

		OverlapResult result;
		result.hit = true;
		result.hitNormal = Math::Vector3::Transform(local.hitNormal, box.orientation);
		result.overlapDistance = local.overlapDistance;
		result.hitPos = Math::Vector3::Transform(local.hitPos, box.orientation) + box.center;
		return result;
	}

	// OBB vs OBB。両方が回転しているため、13軸SAT(AABBVsTriangleの応用)では
	// 済まず、両BOXの主軸(3+3)と、それぞれの主軸同士の外積(9)を合わせた
	// 15軸のSATで判定する(古典的なOBB-OBB分離軸判定)。
	// 押し出しは「重なりが最も小さい軸」をMTV(最小移動ベクトル)として採用する。
	inline OverlapResult OBBVsOBB(const OrientedBox& a, const OrientedBox& b)
	{
		OverlapResult result;

		// 各BOXのワールド空間での主軸(単位ベクトル)
		const Math::Vector3 axesA[3] = {
			Math::Vector3::Transform(Math::Vector3(1,0,0), a.orientation),
			Math::Vector3::Transform(Math::Vector3(0,1,0), a.orientation),
			Math::Vector3::Transform(Math::Vector3(0,0,1), a.orientation),
		};
		const Math::Vector3 axesB[3] = {
			Math::Vector3::Transform(Math::Vector3(1,0,0), b.orientation),
			Math::Vector3::Transform(Math::Vector3(0,1,0), b.orientation),
			Math::Vector3::Transform(Math::Vector3(0,0,1), b.orientation),
		};

		const Math::Vector3 centerDiff = b.center - a.center;

		float minOverlap = std::numeric_limits<float>::max();
		Math::Vector3 minAxis;

		auto testAxis = [&](Math::Vector3 axis) -> bool
		{
			const float axisLenSq = axis.LengthSquared();
			if (axisLenSq < 1e-10f) return true; // 縮退した軸(平行なエッジ同士)は無視

			axis /= std::sqrt(axisLenSq);

			// aをこの軸に投影した半径
			const float radiusA =
				a.halfExtents.x * std::abs(axis.Dot(axesA[0])) +
				a.halfExtents.y * std::abs(axis.Dot(axesA[1])) +
				a.halfExtents.z * std::abs(axis.Dot(axesA[2]));

			const float radiusB =
				b.halfExtents.x * std::abs(axis.Dot(axesB[0])) +
				b.halfExtents.y * std::abs(axis.Dot(axesB[1])) +
				b.halfExtents.z * std::abs(axis.Dot(axesB[2]));

			const float dist = std::abs(centerDiff.Dot(axis));
			const float overlap = (radiusA + radiusB) - dist;

			if (overlap <= 0.0f) return false; // この軸で分離できている = 交差していない

			if (overlap < minOverlap) {
				minOverlap = overlap;
				// 押し出す向きは「aをbから遠ざける」向きに揃える
				minAxis = (centerDiff.Dot(axis) >= 0.0f) ? -axis : axis;
			}
			return true;
		};

		for (const Math::Vector3& axis : axesA) { if (!testAxis(axis)) return result; }
		for (const Math::Vector3& axis : axesB) { if (!testAxis(axis)) return result; }
		for (const Math::Vector3& axisA : axesA) {
			for (const Math::Vector3& axisB : axesB) {
				if (!testAxis(axisA.Cross(axisB))) return result;
			}
		}

		result.hit = true;
		result.hitNormal = minAxis;
		result.overlapDistance = minOverlap;
		result.hitPos = a.center - minAxis * (minOverlap * 0.5f); // 簡易的な接触点近似
		return result;
	}
}
