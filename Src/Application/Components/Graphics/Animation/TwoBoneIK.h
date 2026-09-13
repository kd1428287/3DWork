// TwoBoneIK.h (新規、エンジン依存なし)
#pragma once
#include <algorithm>
#include <cmath>

// ============================================================
// 2ボーン(root-mid-end)構成のIKを解析的に解く、純粋な数学関数。
// 腕(肩-肘-手首)にも脚(股関節-膝-足首)にも同じ形式で使い回せる。
//
// ボーンのバインドポーズ(初期姿勢)がどの軸を向いているかを一切
// 仮定しない。「現在のFKポーズでの各関節の向き」から「求めたい向き」
// への差分回転(最短回転)を計算し、それを現在のワールド回転に
// 合成する方式にしているため(TwoBoneIKComponent::Update参照)。
// ============================================================
namespace TwoBoneIK {

	inline Math::Vector3 SafeNormalize(const Math::Vector3& v, const Math::Vector3& fallback = Math::Vector3::Forward) {
		const float lenSq = v.LengthSquared();
		if (lenSq < 1e-8f) return fallback;
		Math::Vector3 result = v;
		result /= std::sqrt(lenSq);
		return result;
	}

	inline Math::Vector3 RotateAroundAxis(const Math::Vector3& v, const Math::Vector3& axis, float angle) {
		return Math::Vector3::Transform(v, Math::Quaternion::CreateFromAxisAngle(axis, angle));
	}

	// fromからtoへの最短回転(どちらも正規化不要)。
	inline Math::Quaternion ShortestArcRotation(const Math::Vector3& from, const Math::Vector3& to) {
		const Math::Vector3 f = SafeNormalize(from);
		const Math::Vector3 t = SafeNormalize(to);
		const float d = std::clamp(f.Dot(t), -1.0f, 1.0f);

		if (d > 0.99999f) return Math::Quaternion::Identity; // ほぼ同じ方向

		if (d < -0.99999f) {
			// 正反対の方向。回転軸が一意に決まらないため、fと直交する
			// 適当な軸を選んで180度回転させる。
			Math::Vector3 axis = std::abs(f.x) < 0.9f ? f.Cross(Math::Vector3::Right) : f.Cross(Math::Vector3::Up);
			axis = SafeNormalize(axis);
			return Math::Quaternion::CreateFromAxisAngle(axis, static_cast<float>(M_PI));
		}

		Math::Vector3 axis = f.Cross(t);
		axis = SafeNormalize(axis);
		return Math::Quaternion::CreateFromAxisAngle(axis, std::acos(d));
	}

	struct Result {
		Math::Quaternion rootWorldRotation; // root関節(肩/股関節)の新しいワールド回転
		Math::Quaternion midWorldRotation;  // mid関節(肘/膝)の新しいワールド回転
	};

	// rootPos/midPos/endPos     : 現在(FK由来)の各関節の座標(呼び出し側で
	//                             統一した空間、通常はモデル空間かワールド空間)
	// currentRootWorldRotation  : rootの現在のワールド回転(差分合成のベース)
	// currentMidWorldRotation   : midの現在のワールド回転(同上)
	// target                    : 到達させたい終端(手首/足首)の座標
	// poleVector                : 肘/膝がどちら向きに曲がるかを決めるヒント方向
	inline Result Solve(
		const Math::Vector3& rootPos, const Math::Vector3& midPos, const Math::Vector3& endPos,
		const Math::Quaternion& currentRootWorldRotation, const Math::Quaternion& currentMidWorldRotation,
		const Math::Vector3& target, const Math::Vector3& poleVector)
	{
		const float upperLength = (midPos - rootPos).Length();
		const float lowerLength = (endPos - midPos).Length();

		Math::Vector3 toTarget = target - rootPos;
		float targetDistance = toTarget.Length();

		// 届かない/近すぎる特異点をクランプする。
		const float maxReach = upperLength + lowerLength - 0.001f;
		if (targetDistance > maxReach) {
			toTarget = SafeNormalize(toTarget) * maxReach;
			targetDistance = maxReach;
		}
		targetDistance = std::max(targetDistance, std::abs(upperLength - lowerLength) + 0.001f);

		const Math::Vector3 targetDir = toTarget / targetDistance;

		// --- 余弦定理で各関節の角度を求める ------------------------------
		const float cosRootAngle = std::clamp(
			(upperLength * upperLength + targetDistance * targetDistance - lowerLength * lowerLength)
			/ (2.0f * upperLength * targetDistance), -1.0f, 1.0f);
		const float rootAngle = std::acos(cosRootAngle);

		const float cosMidInner = std::clamp(
			(upperLength * upperLength + lowerLength * lowerLength - targetDistance * targetDistance)
			/ (2.0f * upperLength * lowerLength), -1.0f, 1.0f);
		const float midInnerAngle = std::acos(cosMidInner);

		// --- 曲げ平面の法線 ---------------------------------------------
		Math::Vector3 bendAxis = targetDir.Cross(poleVector);
		if (bendAxis.LengthSquared() < 1e-6f) {
			bendAxis = std::abs(targetDir.y) < 0.99f
				? targetDir.Cross(Math::Vector3::Up)
				: targetDir.Cross(Math::Vector3::Right);
		}
		bendAxis = SafeNormalize(bendAxis);

		// --- 「求めたい向き」を、現在の向きとは無関係にtargetDir基準で構築 ---
		const Math::Vector3 desiredRootDir = RotateAroundAxis(targetDir, bendAxis, rootAngle);
		const Math::Vector3 desiredMidDir = RotateAroundAxis(
			desiredRootDir, bendAxis, -(static_cast<float>(M_PI) - midInnerAngle));

		// --- 現在の向きから「求めたい向き」への差分回転を、現在のワールド
		//     回転に合成する。ボーンのバインド軸を一切知らずに済む。
		const Math::Vector3 currentRootDir = SafeNormalize(midPos - rootPos);
		const Math::Vector3 currentMidDir = SafeNormalize(endPos - midPos);

		const Math::Quaternion deltaRoot = ShortestArcRotation(currentRootDir, desiredRootDir);
		const Math::Quaternion deltaMid = ShortestArcRotation(currentMidDir, desiredMidDir);

		Result result;
		result.rootWorldRotation = deltaRoot * currentRootWorldRotation;
		result.midWorldRotation = deltaMid * currentMidWorldRotation;
		return result;
	}
}