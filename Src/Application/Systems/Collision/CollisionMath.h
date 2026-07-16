#pragma once

// ============================================================
// 当たり判定の純粋な幾何計算だけを集めた場所。
//
// KdCollision.cpp(ポリゴン/メッシュ含む低レベル判定関数群)の役割を
// 引き継ぐ部分だが、今回はSphere/AABB/Rayの基本形状のみを対象にする
// (メッシュ・ポリゴン単位の精密判定、OBB(向きを持つ箱)は未対応。
//  必要になったら関数を追加する形で拡張する)。
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
}
