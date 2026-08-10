#pragma once

// ============================================================
// 当たり判定の純粋な幾何計算だけを集めた場所。
//
// KdCollision.cpp(ポリゴン/メッシュ含む低レベル判定関数群)の役割を
// 引き継ぐ部分。Sphere/Box(OBB。回転を考慮するBOX)/Capsule(線分+半径)/
// Rayの基本形状に加え、三角形単位の判定(KdPointToTriangle/DirectX::
// TriangleTests::Intersects相当)もここに集約している。
//
// Capsule絡みの判定(CapsuleVsOBB/CapsuleVsTriangle)は、線分と相手形状の
// 「真の最近接点対」を解析的に一発で求めるのではなく、"交互射影法"
// (線分側→相手側→線分側…と最近接点を数回往復させ収束させる近似)で
// 求めている。線分・OBB・三角形はいずれも凸形状なので、この方法は
// 数回の反復で実用上十分な精度に収束する(Capsule vs Capsuleのみ、
// 線分同士の最近接点対を解析的に一発で求める古典的な閉じた式
// (ClosestPtSegmentSegment)があるため、そちらを使っている)。
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

	// 点から線分への最近接点。Capsule(線分+半径)の判定全般で使う基礎関数。
	inline Math::Vector3 ClosestPointOnSegment(
		const Math::Vector3& p, const Math::Vector3& a, const Math::Vector3& b)
	{
		const Math::Vector3 ab = b - a;
		const float abLenSq = ab.LengthSquared();
		if (abLenSq < 1e-10f) return a; // 縮退(始点=終点。実質ただの点)

		float t = (p - a).Dot(ab) / abLenSq;
		t = std::clamp(t, 0.0f, 1.0f);
		return a + ab * t;
	}

	// 線分同士の最近接点対を求める(Christer Ericson「リアルタイム衝突判定」の
	// ClosestPtSegmentSegmentと同一のアルゴリズム)。Capsule vs Capsuleの
	// 判定で、両カプセルの軸線分がどこで最も近づくかを求めるために使う。
	inline void ClosestPointSegmentSegment(
		const Math::Vector3& p1, const Math::Vector3& q1,
		const Math::Vector3& p2, const Math::Vector3& q2,
		Math::Vector3& outC1, Math::Vector3& outC2)
	{
		const Math::Vector3 d1 = q1 - p1;
		const Math::Vector3 d2 = q2 - p2;
		const Math::Vector3 r = p1 - p2;
		const float a = d1.Dot(d1);
		const float e = d2.Dot(d2);
		const float f = d2.Dot(r);

		constexpr float kEpsilon = 1e-8f;
		float s, t;

		if (a <= kEpsilon && e <= kEpsilon) {
			// 両方とも点に縮退している
			outC1 = p1;
			outC2 = p2;
			return;
		}

		if (a <= kEpsilon) {
			s = 0.0f;
			t = std::clamp(f / e, 0.0f, 1.0f);
		}
		else {
			const float c = d1.Dot(r);
			if (e <= kEpsilon) {
				t = 0.0f;
				s = std::clamp(-c / a, 0.0f, 1.0f);
			}
			else {
				const float b = d1.Dot(d2);
				const float denom = a * e - b * b;

				// denomが0に近い(2本の線分がほぼ平行)場合はs=0を基準にする。
				s = (denom > kEpsilon) ? std::clamp((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;

				t = (b * s + f) / e;

				// tが[0,1]の外に出た場合、その端点に固定してsを求め直す
				// (Ericson本のロバスト化版と同じ処理)。
				if (t < 0.0f) {
					t = 0.0f;
					s = std::clamp(-c / a, 0.0f, 1.0f);
				}
				else if (t > 1.0f) {
					t = 1.0f;
					s = std::clamp((b - c) / a, 0.0f, 1.0f);
				}
			}
		}

		outC1 = p1 + d1 * s;
		outC2 = p2 + d2 * t;
	}

	// ============================================================
	// Capsule(線分+半径。カプセルコライダー)の判定。
	//
	// キャラクターの主判定形状として、Sphere(不均一スケールで歪む/
	// 段差に引っかかりやすい)やBox(角に引っかかりやすい)よりも
	// 段差・斜面追従が滑らかなため、Unity(CapsuleCollider)/
	// Unreal(UCapsuleComponent)ともにキャラクター用のデフォルト形状
	// として採用している。
	//
	// 中身は「線分(start-end)の周りに半径radiusを膨らませた形状」
	// というだけなので、他形状との判定は基本的に「まず線分側の
	// 最近接点(あるいは最近接点対)を求め、そこから先はSphereの判定
	// 関数に委譲する」という形に帰着できる(半径付きの点=球なので)。
	// ============================================================
	struct Capsule
	{
		Math::Vector3	start{};
		Math::Vector3	end{};
		float			radius = 0.5f;
	};

	// Capsule vs Capsule。両カプセルの軸線分同士の最近接点対を求め、
	// そこから先はSphereVsSphereと全く同じロジック(距離と半径の和の比較)。
	inline OverlapResult CapsuleVsCapsule(const Capsule& a, const Capsule& b)
	{
		Math::Vector3 c1, c2;
		ClosestPointSegmentSegment(a.start, a.end, b.start, b.end, c1, c2);
		return SphereVsSphere(c1, a.radius, c2, b.radius);
	}

	// Sphere vs Capsule。球中心からカプセルの軸線分への最近接点を求めれば、
	// あとは球同士の判定(SphereVsSphere)に帰着する。
	// hitNormalは「sphereをcapsuleから押し出す向き」。
	inline OverlapResult SphereVsCapsule(
		const Math::Vector3& sphereCenter, float sphereRadius, const Capsule& capsule)
	{
		const Math::Vector3 nearest = ClosestPointOnSegment(sphereCenter, capsule.start, capsule.end);
		return SphereVsSphere(sphereCenter, sphereRadius, nearest, capsule.radius);
	}

	// レイ vs Capsule。円柱の側面(無限円柱として交点を求め、線分の範囲内かで
	// 絞り込む)+両端の半球(RayVsSphereをそのまま流用)、の3パーツに分けて
	// 判定し、最も手前のヒットを採用する。
	// ※レイの発射点がカプセル内部から始まっているケースは円柱側面については
	//   厳密に扱えていない(RayVsAABBと同様の簡易実装。発射点が内部にある
	//   状況は「攻撃判定の中に元々いる」ような特殊ケースなので、実用上は
	//   Enter/Exit判定(CollisionSystem側)で別途カバーされる想定)。
	inline RayHitResult RayVsCapsule(
		const Math::Vector3& rayOrigin, const Math::Vector3& rayDir, float rayRange,
		const Capsule& capsule)
	{
		RayHitResult result;

		const Math::Vector3 axis = capsule.end - capsule.start;
		const float axisLenSq = axis.LengthSquared();

		// 縮退(start==end): 実質ただの球
		if (axisLenSq < 1e-10f) {
			return RayVsSphere(rayOrigin, rayDir, rayRange, capsule.start, capsule.radius);
		}

		const float axisLen = std::sqrt(axisLenSq);
		const Math::Vector3 axisDir = axis / axisLen;

		// レイ・原点それぞれをaxisDirに垂直な平面へ投影し、円柱断面の
		// 2次元の円との交差問題に落とし込む。
		const Math::Vector3 originToStart = rayOrigin - capsule.start;
		const float rayDotAxis = rayDir.Dot(axisDir);
		const float originDotAxis = originToStart.Dot(axisDir);

		const Math::Vector3 rayPerp = rayDir - axisDir * rayDotAxis;
		const Math::Vector3 originPerp = originToStart - axisDir * originDotAxis;

		const float qa = rayPerp.LengthSquared();
		const float qb = 2.0f * rayPerp.Dot(originPerp);
		const float qc = originPerp.LengthSquared() - capsule.radius * capsule.radius;

		float bestDist = rayRange;
		bool found = false;
		Math::Vector3 bestPos, bestNormal;

		// 円柱の側面
		if (qa > 1e-10f) {
			const float disc = qb * qb - 4.0f * qa * qc;
			if (disc >= 0.0f) {
				const float sqrtDisc = std::sqrt(disc);
				const float candidates[2] = { (-qb - sqrtDisc) / (2.0f * qa), (-qb + sqrtDisc) / (2.0f * qa) };

				for (const float t : candidates) {
					if (t < 0.0f || t > bestDist) continue;

					const float hAlongAxis = originDotAxis + rayDotAxis * t;
					if (hAlongAxis < 0.0f || hAlongAxis > axisLen) continue; // 円柱の範囲外(半球側で別途判定)

					bestDist = t;
					found = true;
					bestPos = rayOrigin + rayDir * t;
					const Math::Vector3 onAxis = capsule.start + axisDir * hAlongAxis;
					bestNormal = bestPos - onAxis;
					bestNormal.Normalize();
				}
			}
		}

		// 両端の半球
		const Math::Vector3 capCenters[2] = { capsule.start, capsule.end };
		for (const Math::Vector3& capCenter : capCenters) {
			const RayHitResult capHit = RayVsSphere(rayOrigin, rayDir, bestDist, capCenter, capsule.radius);
			if (capHit.hit && capHit.distance <= bestDist) {
				bestDist = capHit.distance;
				found = true;
				bestPos = capHit.hitPos;
				bestNormal = capHit.hitNormal;
			}
		}

		if (!found) return result;

		result.hit = true;
		result.distance = bestDist;
		result.hitPos = bestPos;
		result.hitNormal = bestNormal;
		return result;
	}


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

	// Capsule vs 三角形1枚。線分と三角形はどちらも凸形状なので、
	// 「線分上の点から三角形への最近接点」→「その点から線分への最近接点」
	// を数回往復させる(交互射影法)だけで真の最近接点対に収束する。
	// 収束後はSphereVsTriangleにそのまま委譲すればよい
	// (収束済みの点を中心とする半径radiusの球、として扱えるため)。
	inline OverlapResult CapsuleVsTriangle(
		const Capsule& capsule, const Math::Vector3& v0, const Math::Vector3& v1, const Math::Vector3& v2)
	{
		Math::Vector3 pointOnSegment = (capsule.start + capsule.end) * 0.5f;

		constexpr int kIterations = 4; // 凸形状同士の交互射影は数回で十分収束する
		for (int i = 0; i < kIterations; ++i) {
			const Math::Vector3 pointOnTriangle = ClosestPointOnTriangle(pointOnSegment, v0, v1, v2);
			pointOnSegment = ClosestPointOnSegment(pointOnTriangle, capsule.start, capsule.end);
		}

		return SphereVsTriangle(pointOnSegment, capsule.radius, v0, v1, v2);
	}


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

	// Capsule vs OBB。BOXのローカル空間(回転を打ち消した空間)へ線分の
	// 両端だけ持ち込み、「線分上の点をBOXの範囲にクランプ→そのクランプ
	// 結果に最も近い線分上の点」を数回往復させて最近接点対に近似収束させる
	// (CapsuleVsTriangleと同じ交互射影の考え方。BOXは各軸へのクランプが
	// そのまま最近接点になる凸形状なので、三角形の場合よりむしろ単純)。
	// 収束後はSphereVsAABBに委譲し、結果をワールド空間へ戻す。
	inline OverlapResult CapsuleVsOBB(const Capsule& capsule, const OrientedBox& box)
	{
		Math::Quaternion invRot;
		box.orientation.Conjugate(invRot);

		const Math::Vector3 localStart = Math::Vector3::Transform(capsule.start - box.center, invRot);
		const Math::Vector3 localEnd = Math::Vector3::Transform(capsule.end - box.center, invRot);

		Math::Vector3 pointOnSegment = (localStart + localEnd) * 0.5f;

		constexpr int kIterations = 4;
		for (int i = 0; i < kIterations; ++i) {
			const Math::Vector3 pointOnBox = Math::Vector3(
				std::clamp(pointOnSegment.x, -box.halfExtents.x, box.halfExtents.x),
				std::clamp(pointOnSegment.y, -box.halfExtents.y, box.halfExtents.y),
				std::clamp(pointOnSegment.z, -box.halfExtents.z, box.halfExtents.z));

			pointOnSegment = ClosestPointOnSegment(pointOnBox, localStart, localEnd);
		}

		const OverlapResult local = SphereVsAABB(pointOnSegment, capsule.radius, Math::Vector3::Zero, box.halfExtents);
		if (!local.hit) return local;

		OverlapResult result;
		result.hit = true;
		result.hitNormal = Math::Vector3::Transform(local.hitNormal, box.orientation);
		result.overlapDistance = local.overlapDistance;
		result.hitPos = Math::Vector3::Transform(local.hitPos, box.orientation) + box.center;
		return result;
	}


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