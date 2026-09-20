#include "Application/main.h"
#include "SlashTrailInstance.h"

namespace
{
	// 0～1にクランプした上でスムーズな(両端で傾き0の)補間を行う
	float Smoothstep01(float x)
	{
		x = std::clamp(x, 0.0f, 1.0f);
		return x * x * (3.0f - 2.0f * x);
	}

	// Catmull-Romスプライン(uniform)。P1→P2間をtで補間する。P0/P3は前後の制御点
	DirectX::SimpleMath::Vector3 CatmullRom(
		const DirectX::SimpleMath::Vector3& P0, const DirectX::SimpleMath::Vector3& P1,
		const DirectX::SimpleMath::Vector3& P2, const DirectX::SimpleMath::Vector3& P3,
		float t)
	{
		const float t2 = t * t;
		const float t3 = t2 * t;

		return 0.5f * (
			(2.0f * P1) +
			(-P0 + P2) * t +
			(2.0f * P0 - 5.0f * P1 + 4.0f * P2 - P3) * t2 +
			(-P0 + 3.0f * P1 - 3.0f * P2 + P3) * t3);
	}

	// スプライン補間・幅計算より後段の処理で使う、細分化済みの1点
	struct ResampledPoint
	{
		DirectX::SimpleMath::Vector3	Tip;
		DirectX::SimpleMath::Vector3	Base;
		float							Age = 0.0f;
		float							Speed = 0.0f;
	};
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
	hasCurrentTip_ = false;
	hasSmoothedSpeed_ = false;
	smoothedSpeed_ = 0.0f;
	hasSmoothedTipOffset_ = false;
	smoothedTipOffset_ = 0.0f;
	sinceLastSampleTime_ = 0.0f;

	const UINT vertexCapacityPoints = (params_.SplineMaxPoints > params_.MaxSamples)
		? params_.SplineMaxPoints : params_.MaxSamples;

	renderer_ = std::make_shared<SlashTrailRenderer>();
	if (!renderer_->Init(vertexCapacityPoints * 2))
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
	hasCurrentTip_ = false;
	hasSmoothedSpeed_ = false;
	smoothedSpeed_ = 0.0f;
	hasSmoothedTipOffset_ = false;
	smoothedTipOffset_ = 0.0f;
	sinceLastSampleTime_ = 0.0f;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 記録中、毎フレーム剣のTip/Base座標を渡す
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::UpdateTipBase(const DirectX::SimpleMath::Vector3& tip, const DirectX::SimpleMath::Vector3& base)
{
	if (!isRecording_) { return; }

	currentTip_ = tip;
	hasCurrentTip_ = true;

	const float minDistSq = params_.MinSampleDistance * params_.MinSampleDistance;
	const bool shouldRecord = !hasLastRecordedTip_ ||
		(tip - lastRecordedTip_).LengthSquared() >= minDistSq;

	if (!shouldRecord) { return; }

	float rawSpeed = params_.SpeedWidthReference;
	if (hasLastRecordedTip_)
	{
		const float dist = (tip - lastRecordedTip_).Length();
		const float dt = (sinceLastSampleTime_ > 1e-5f) ? sinceLastSampleTime_ : 1e-5f;
		rawSpeed = dist / dt;
	}

	if (hasSmoothedSpeed_)
	{
		const float alpha = std::clamp(params_.SpeedSmoothingAlpha, 0.0f, 1.0f);
		smoothedSpeed_ = std::lerp(smoothedSpeed_, rawSpeed, alpha);
	}
	else
	{
		smoothedSpeed_ = rawSpeed;
		hasSmoothedSpeed_ = true;
	}

	SlashTrailSample sample;
	sample.Tip = tip;
	sample.Base = base;
	sample.Age = 0.0f;
	sample.Speed = smoothedSpeed_;
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
// 毎フレーム更新：Age加算→間引き→tipOffsetのEMA平滑化→頂点再構築
//
//	【tipOffsetの平滑化】
//	rawTipOffset = 「直近に記録されたサンプル(samples_.back())→現在の実際の剣先
//	(currentTip_)」の距離。記録中は毎フレーム連続的に伸びるが、振りが速いフレームほど
//	1フレームあたりの伸び量が大きくなり、これがそのままUVの進行量になると
//	非等幅なテクスチャの模様が急激に切り替わって見える(速度に比例して悪化する
//	太さの脈動の原因)。EMAでこの値を滑らかにしてからRebuildVertices()へ渡す
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::Update(float deltaTime)
{
	sinceLastSampleTime_ += deltaTime;

	for (auto& sample : samples_)
	{
		sample.Age += deltaTime;
	}

	TrimExpiredSamples();

	if (!samples_.empty() && hasCurrentTip_)
	{
		const float rawTipOffset = (currentTip_ - samples_.back().Tip).Length();

		if (hasSmoothedTipOffset_)
		{
			const float alpha = std::clamp(params_.TipOffsetSmoothingAlpha, 0.0f, 1.0f);
			smoothedTipOffset_ = std::lerp(smoothedTipOffset_, rawTipOffset, alpha);
		}
		else
		{
			smoothedTipOffset_ = rawTipOffset;
			hasSmoothedTipOffset_ = true;
		}
	}
	else
	{
		// サンプルが無い、またはまだ剣先座標を受け取っていない場合はリセットしておく
		smoothedTipOffset_ = 0.0f;
		hasSmoothedTipOffset_ = false;
	}

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
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::RebuildVertices()
{
	vertices_.clear();
	coreVertices_.clear();

	const size_t sampleCount = samples_.size();
	if (sampleCount < 2) { return; }

	using namespace DirectX::SimpleMath;

	// ----- 1. スプラインによる細分化(resampledを構築) -----
	std::vector<ResampledPoint> resampled;

	if (params_.SplineEnabled)
	{
		const size_t segmentCount = sampleCount - 1;
		UINT subdiv = (params_.SplineSubdivisions > 0) ? params_.SplineSubdivisions : 1;
		if (params_.SplineMaxPoints > 1 && segmentCount > 0)
		{
			const UINT maxSubdivBySegment =
				static_cast<UINT>((params_.SplineMaxPoints - 1) / segmentCount);
			if (maxSubdivBySegment < 1) { subdiv = 1; }
			else if (subdiv > maxSubdivBySegment) { subdiv = maxSubdivBySegment; }
		}

		resampled.reserve(segmentCount * subdiv + 1);

		for (size_t i = 0; i < segmentCount; ++i)
		{
			const SlashTrailSample& s0 = samples_[(i == 0) ? 0 : i - 1];
			const SlashTrailSample& s1 = samples_[i];
			const SlashTrailSample& s2 = samples_[i + 1];
			const SlashTrailSample& s3 = samples_[(i + 2 < sampleCount) ? i + 2 : sampleCount - 1];

			const UINT stepsThisSegment = (i + 1 == segmentCount) ? subdiv : subdiv - 1;

			for (UINT step = 0; step <= stepsThisSegment; ++step)
			{
				const float t = static_cast<float>(step) / static_cast<float>(subdiv);

				ResampledPoint p;
				p.Tip = CatmullRom(s0.Tip, s1.Tip, s2.Tip, s3.Tip, t);
				p.Base = CatmullRom(s0.Base, s1.Base, s2.Base, s3.Base, t);
				p.Age = std::lerp(s1.Age, s2.Age, t);
				p.Speed = std::lerp(s1.Speed, s2.Speed, t);
				resampled.push_back(p);
			}
		}
	}
	else
	{
		resampled.reserve(sampleCount);
		for (const auto& s : samples_)
		{
			ResampledPoint p;
			p.Tip = s.Tip;
			p.Base = s.Base;
			p.Age = s.Age;
			p.Speed = s.Speed;
			resampled.push_back(p);
		}
	}

	const size_t pointCount = resampled.size();
	if (pointCount < 2) { return; }

	vertices_.reserve(pointCount * 2);
	coreVertices_.reserve(pointCount * 2);

	const float fadeLengthSafe = (params_.FadeLength > 0.0f) ? params_.FadeLength : 0.0001f;
	const float speedRefSafe = (params_.SpeedWidthReference > 0.0f) ? params_.SpeedWidthReference : 0.0001f;
	const float tileLengthSafe = (params_.UVTileLength > 0.0f) ? params_.UVTileLength : 0.0001f;

	const float tPeak = std::clamp(params_.BleedPeakTime / fadeLengthSafe, 0.01f, 0.99f);

	// ----- 距離ベースUV用:最新(back)からの累積距離を、細分化後の点列で計算し直す -----
	std::vector<float> cumDist(pointCount, 0.0f);
	for (size_t idx = pointCount - 1; idx-- > 0; )
	{
		const float segment = (resampled[idx + 1].Tip - resampled[idx].Tip).Length();
		cumDist[idx] = cumDist[idx + 1] + segment;
	}

	// ----- tipOffsetはUpdate()で既にEMA平滑化済みの値をそのまま使う -----
	const float tipOffset = smoothedTipOffset_;

	const Vector3 camPos = KdShaderManager::Instance().GetCameraCB().CamPos;

	for (size_t idx = 0; idx < pointCount; ++idx)
	{
		const ResampledPoint& sample = resampled[idx];

		const float fadeRate = std::clamp(1.0f - sample.Age / fadeLengthSafe, 0.0f, 1.0f);

		float u = fadeRate;
		if (params_.UseDistanceBasedUV)
		{
			u = fmodf((cumDist[idx] + tipOffset) / tileLengthSafe, 1.0f);
			if (u < 0.0f) { u += 1.0f; }
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

			const float fade = std::sqrt(2.0f * fadeRate - fadeRate * fadeRate);

			SlashTrailVertex vTipCore;
			vTipCore.Position = tipCore;
			vTipCore.UV = { u, 0.0f };
			vTipCore.Color = { params_.CoreColor.x, params_.CoreColor.y, params_.CoreColor.z, fade - 0.15f };
			coreVertices_.push_back(vTipCore);

			SlashTrailVertex vBaseCore;
			vBaseCore.Position = baseCore;
			vBaseCore.UV = { u, 1.0f };
			vBaseCore.Color = { params_.CoreColor.x, params_.CoreColor.y, params_.CoreColor.z, fade - 0.15f };
			coreVertices_.push_back(vBaseCore);
		}
	}
}