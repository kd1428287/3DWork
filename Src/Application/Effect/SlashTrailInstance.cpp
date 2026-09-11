#include "../main.h"
#include "SlashTrailInstance.h"

#include <algorithm>
#include <vector>

namespace
{
	// 0～1にクランプした上でスムーズな(両端で傾き0の)補間を行う
	float Smoothstep01(float x)
	{
		x = std::clamp(x, 0.0f, 1.0f);
		return x * x * (3.0f - 2.0f * x);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 初期化：パラメータを保持し、GPU描画用リソース(KdSlashTrailRenderer)を生成する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool SlashTrailInstance::Init(const SlashTrailParams& params)
{
	params_ = params;

	samples_.clear();
	vertices_.clear();
	coreVertices_.clear();

	isRecording_ = false;
	hasLastRecordedTip_ = false;
	sinceLastSampleTime_ = 0.0f;

	renderer_ = std::make_shared<SlashTrailRenderer>();
	if (!renderer_->Init(params_.MaxSamples * 2))
	{
		renderer_.reset();
		return false;
	}

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 記録開始：既存サンプルを破棄してゼロから記録し直す
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::BeginRecording()
{
	samples_.clear();
	vertices_.clear();
	coreVertices_.clear();

	isRecording_ = true;
	hasLastRecordedTip_ = false;
	sinceLastSampleTime_ = 0.0f;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 記録中、毎フレーム剣のTip/Base座標を渡す
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::UpdateTipBase(const DirectX::SimpleMath::Vector3& tip, const DirectX::SimpleMath::Vector3& base)
{
	if (!isRecording_) { return; }

	const float minDistSq = params_.MinSampleDistance * params_.MinSampleDistance;
	const bool shouldRecord = !hasLastRecordedTip_ ||
		(tip - lastRecordedTip_).LengthSquared() >= minDistSq;

	if (!shouldRecord) { return; }

	float speed = params_.SpeedWidthReference;
	if (hasLastRecordedTip_)
	{
		const float dist = (tip - lastRecordedTip_).Length();
		const float dt = (sinceLastSampleTime_ > 1e-5f) ? sinceLastSampleTime_ : 1e-5f;
		speed = dist / dt;
	}

	SlashTrailSample sample;
	sample.Tip = tip;
	sample.Base = base;
	sample.Age = 0.0f;
	sample.Speed = speed;
	samples_.push_back(sample);

	lastRecordedTip_ = tip;
	hasLastRecordedTip_ = true;
	sinceLastSampleTime_ = 0.0f;

	TrimOverflowSamples();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 記録終了
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::EndRecording()
{
	isRecording_ = false;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 毎フレーム更新
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::Update(float deltaTime)
{
	sinceLastSampleTime_ += deltaTime;

	for (auto& sample : samples_)
	{
		sample.Age += deltaTime;
	}

	TrimExpiredSamples();
	RebuildVertices();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画：墨レイヤー→芯レイヤーの順に、同じrenderer_を使い回して2回描画する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::Draw(ParticleDrawPass pass) const
{
	if (!renderer_) { return; }

	if (KdHasDrawPassFlag(params_.DrawPassFlags, pass))
	{
		renderer_->Draw(vertices_, params_.BlendMode);
	}

	if (params_.CoreEnabled && KdHasDrawPassFlag(params_.CoreDrawPassFlags, pass))
	{
		renderer_->Draw(coreVertices_, params_.CoreBlendMode);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::TrimExpiredSamples()
{
	while (!samples_.empty() && samples_.front().Age > params_.FadeLength)
	{
		samples_.pop_front();
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::TrimOverflowSamples()
{
	while (samples_.size() > params_.MaxSamples)
	{
		samples_.pop_front();
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// samples_から、墨レイヤー(vertices_)と芯レイヤー(coreVertices_)の頂点配列を作り直す
//
//	【UV.xの決め方(距離ベースのタイリング)】
//	従来はUV.x = fadeRate(Age/FadeLengthの正規化値)を使い、トレイル全体の長さに
//	関わらず常に0～1へ引き伸ばしていた。これだと非等幅なテクスチャ(墨の一筆)を
//	使った時、トレイルの物理的な長さが振りの速さで変わるたびにテクスチャの
//	太さパターンまで伸び縮みして見えてしまう(「電動ノコギリ」現象)。
//
//	対策として、samples_を最新(back)から最古(front)へ向かって走査し、
//	「現在の切っ先(最新サンプルのTip)からの累積距離」を各サンプルごとに求め、
//	UV.x = fmod(累積距離 / UVTileLength, 1.0) とする。これによりテクスチャの
//	1パターンが常に一定の物理距離に対応するようになり、トレイルが伸び縮みしても
//	パターンの繰り返し回数が増減するだけで、個々のパターンの太さ自体は変化しない。
//	最新サンプルを基準点(距離0)に固定しているため、フレームが進んで新しい
//	サンプルが追加されても、既存部分の距離基準がずれてパターンが滑って見える
//	(いわゆる「泳ぐ」)現象も起きない
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::RebuildVertices()
{
	vertices_.clear();
	coreVertices_.clear();

	const size_t sampleCount = samples_.size();
	if (sampleCount < 2) { return; }

	vertices_.reserve(sampleCount * 2);
	coreVertices_.reserve(sampleCount * 2);

	using namespace DirectX::SimpleMath;

	const float fadeLengthSafe = (params_.FadeLength > 0.0f) ? params_.FadeLength : 0.0001f;
	const float speedRefSafe = (params_.SpeedWidthReference > 0.0f) ? params_.SpeedWidthReference : 0.0001f;
	const float tileLengthSafe = (params_.UVTileLength > 0.0f) ? params_.UVTileLength : 0.0001f;

	const float tPeak = std::clamp(params_.BleedPeakTime / fadeLengthSafe, 0.01f, 0.99f);

	// ----- 距離ベースUV用:最新(back)からの累積距離を先に計算しておく -----
	//	cumDist[i] = samples_[i]からsamples_[末尾](最新の切っ先)までの経路長
	std::vector<float> cumDist(sampleCount, 0.0f);
	for (size_t idx = sampleCount - 1; idx-- > 0; )
	{
		// idxはsampleCount-2から0まで降順で回る(idx-- > 0の性質を利用)
		const float segment = (samples_[idx + 1].Tip - samples_[idx].Tip).Length();
		cumDist[idx] = cumDist[idx + 1] + segment;
	}

	const Vector3 camPos = KdShaderManager::Instance().GetCameraCB().CamPos;

	for (size_t idx = 0; idx < sampleCount; ++idx)
	{
		const SlashTrailSample& sample = samples_[idx];

		const float fadeRate = std::clamp(1.0f - sample.Age / fadeLengthSafe, 0.0f, 1.0f);

		// ----- UV.xの決定(距離ベース or 従来の時間ベース) -----
		float u = fadeRate;
		if (params_.UseDistanceBasedUV)
		{
			u = fmodf(cumDist[idx] / tileLengthSafe, 1.0f);
			if (u < 0.0f) { u += 1.0f; }	// fmodfは負値を返す事があるための保険
		}

		const Vector3 color = Vector3::Lerp(params_.ColdColor, params_.HotColor, fadeRate);

		const float taper = 1.0f - (1.0f - fadeRate) * params_.TipWidthTaper;

		const float speedScale = std::clamp(
			sample.Speed / speedRefSafe,
			params_.MinSpeedWidthScale, params_.MaxSpeedWidthScale);

		float bleedScale = 1.0f;
		if (params_.BleedEnabled)
		{
			const float t = std::clamp(sample.Age / fadeLengthSafe, 0.0f, 1.0f);
			if (t < tPeak)
			{
				bleedScale = std::lerp(params_.BleedStartScale, params_.BleedPeakScale, Smoothstep01(t / tPeak));
			}
			else
			{
				const float decayT = (t - tPeak) / (1.0f - tPeak);
				bleedScale = std::lerp(params_.BleedPeakScale, 1.0f, Smoothstep01(decayT));
			}
		}

		const Vector3 center = (sample.Tip + sample.Base) * 0.5f;

		// ----- カメラ正対補正 -----
		Vector3 widthVec = sample.Tip - sample.Base;
		const float widthLen = widthVec.Length();

		Vector3 correctedWidthVec = widthVec;
		if (widthLen > 1e-5f)
		{
			Vector3 viewDir = camPos - center;
			const float viewDirLenSq = viewDir.LengthSquared();
			if (viewDirLenSq > 1e-6f)
			{
				viewDir /= sqrtf(viewDirLenSq);

				Vector3 camRight = viewDir.Cross(Vector3::Up);
				if (camRight.LengthSquared() < 1e-6f)
				{
					camRight = viewDir.Cross(Vector3::Right);
				}
				camRight.Normalize();

				if (widthVec.Dot(camRight) < 0.0f) { camRight = -camRight; }

				const float alignment = fabsf(widthVec.Dot(viewDir)) / widthLen;

				correctedWidthVec = Vector3::Lerp(widthVec, camRight * widthLen, alignment);
			}
		}

		correctedWidthVec *= speedScale * bleedScale;

		const Vector3 halfWidth = correctedWidthVec * 0.5f;
		const Vector3 tipAdj = Vector3::Lerp(center, center + halfWidth, taper);
		const Vector3 baseAdj = Vector3::Lerp(center, center - halfWidth, taper);

		SlashTrailVertex vTip;
		vTip.Position = tipAdj;
		vTip.UV = { u, 0.0f };
		vTip.Color = { color.x, color.y, color.z, fadeRate };
		vertices_.push_back(vTip);

		SlashTrailVertex vBase;
		vBase.Position = baseAdj;
		vBase.UV = { u, 1.0f };
		vBase.Color = { color.x, color.y, color.z, fadeRate };
		vertices_.push_back(vBase);

		if (params_.CoreEnabled)
		{
			const Vector3 coreHalfWidth = halfWidth * params_.CoreWidthScale;
			const Vector3 tipCore = Vector3::Lerp(center, center + coreHalfWidth, taper);
			const Vector3 baseCore = Vector3::Lerp(center, center - coreHalfWidth, taper);

			SlashTrailVertex vTipCore;
			vTipCore.Position = tipCore;
			vTipCore.UV = { u, 0.0f };
			vTipCore.Color = { params_.CoreColor.x, params_.CoreColor.y, params_.CoreColor.z, fadeRate };
			coreVertices_.push_back(vTipCore);

			SlashTrailVertex vBaseCore;
			vBaseCore.Position = baseCore;
			vBaseCore.UV = { u, 1.0f };
			vBaseCore.Color = { params_.CoreColor.x, params_.CoreColor.y, params_.CoreColor.z, fadeRate };
			coreVertices_.push_back(vBaseCore);
		}
	}
}