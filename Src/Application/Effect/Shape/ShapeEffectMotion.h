#pragma once

#include <vector>
#include <algorithm>
#include <cmath>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 一枚テクスチャエフェクトの動き
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 基準点(行列)のローカル空間で姿勢を返す。ワールド化はComposeWorld()で行う。
// 手続き型の動き(周回・螺旋等)は、IEffectMotionを継承して別実装にする。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

enum class EaseType
{
	Linear,
	InQuad, OutQuad, InOutQuad,
	InCubic, OutCubic, InOutCubic,
};

// 0〜1のtにイージングをかける(範囲外はクランプ)
inline float ApplyEase(EaseType type, float t)
{
	t = std::clamp(t, 0.0f, 1.0f);
	switch (type)
	{
	case EaseType::InQuad:     return t * t;
	case EaseType::OutQuad:    return t * (2.0f - t);
	case EaseType::InOutQuad:  return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
	case EaseType::InCubic:    return t * t * t;
	case EaseType::OutCubic:   { const float u = 1.0f - t; return 1.0f - u * u * u; }
	case EaseType::InOutCubic: return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
	default:                   return t;
	}
}

// 評価結果の姿勢(基準点のローカル空間)
struct EffectPose
{
	Math::Vector3	pos = { 0, 0, 0 };
	Math::Vector3	rotDeg = { 0, 0, 0 };	// x=pitch, y=yaw, z=roll(度)
	Math::Vector2	scale = { 1, 1 };
	float			alpha = 1.0f;
};

// キー：「前のキーからこのキーへ、easeでduration秒かけて到達する」
struct EffectKey
{
	Math::Vector3	pos = { 0, 0, 0 };		// 基準点からの相対位置
	Math::Vector3	rotDeg = { 0, 0, 0 };
	Math::Vector2	scale = { 1, 1 };
	float			alpha = 1.0f;

	EaseType		ease = EaseType::Linear;
	float			duration = 0.0f;		// 先頭キー(初期姿勢)では無視される
};

class IEffectMotion
{
public:
	virtual ~IEffectMotion() = default;

	// 動き全体の長さ(秒)
	virtual float GetDuration() const = 0;

	// 経過時間timeの姿勢。累積時間から求めるので、シークや時間スケールの変化に強い
	virtual EffectPose Evaluate(float time) const = 0;
};

// キー列による動き。keys[0]を初期姿勢とし、以降を前から順に消化する
class KeyframeMotion : public IEffectMotion
{
public:

	std::vector<EffectKey> keys;

	float GetDuration() const override
	{
		float total = 0.0f;
		for (size_t i = 1; i < keys.size(); ++i) { total += keys[i].duration; }
		return total;
	}

	EffectPose Evaluate(float time) const override
	{
		if (keys.empty()) { return EffectPose{}; }

		float start = 0.0f;
		for (size_t i = 1; i < keys.size(); ++i)
		{
			const EffectKey& to = keys[i];
			const float end = start + to.duration;

			// duration=0のキーはここで飛ばされ、次のセグメントの始点になる
			if (time < end)
			{
				const float t = (to.duration > 0.0f) ? (time - start) / to.duration : 1.0f;
				return Lerp(keys[i - 1], to, ApplyEase(to.ease, t));
			}
			start = end;
		}

		// 終端以降は最終姿勢を保持
		return ToPose(keys.back());
	}

private:

	static EffectPose ToPose(const EffectKey& k)
	{
		return EffectPose{ k.pos, k.rotDeg, k.scale, k.alpha };
	}

	// 回転は角度のまま線形補間する(180度を超える回転もできる)
	static EffectPose Lerp(const EffectKey& a, const EffectKey& b, float t)
	{
		EffectPose p;
		p.pos = Math::Vector3::Lerp(a.pos, b.pos, t);
		p.rotDeg = Math::Vector3::Lerp(a.rotDeg, b.rotDeg, t);
		p.scale = Math::Vector2::Lerp(a.scale, b.scale, t);
		p.alpha = a.alpha + (b.alpha - a.alpha) * t;
		return p;
	}
};

// ローカル姿勢を基準点行列でワールド化する。baseはスケールを含まない回転+平行移動を想定
inline Math::Matrix ComposeWorld(const EffectPose& pose, const Math::Matrix& base)
{
	const Math::Matrix local =
		Math::Matrix::CreateScale(pose.scale.x, pose.scale.y, 1.0f)
		* Math::Matrix::CreateFromYawPitchRoll(
			DirectX::XMConvertToRadians(pose.rotDeg.y),
			DirectX::XMConvertToRadians(pose.rotDeg.x),
			DirectX::XMConvertToRadians(pose.rotDeg.z))
		* Math::Matrix::CreateTranslation(pose.pos);

	return local * base;
}
