#include "Application/main.h"
#include "SlashTrailInstance.h"

namespace
{
	// 記録中にSetLivePathが来ないままこの時間(ゲーム時間)が過ぎたら、強制的に消去へ移す
	constexpr float kLiveTimeout = 0.5f;

	// 整数からワールド単位のずらし量(0〜100)を作る簡易ハッシュ
	float HashOffset(uint32_t x)
	{
		x ^= x >> 16; x *= 0x7feb352dU;
		x ^= x >> 15; x *= 0x846ca68bU;
		x ^= x >> 16;
		return static_cast<float>(x & 0xFFFFFFu) / static_cast<float>(0x1000000u) * 100.0f;
	}

	// 0～1にクランプした上で、両端で傾き0の補間を行う
	float Smoothstep01(float x)
	{
		x = std::clamp(x, 0.0f, 1.0f);
		return x * x * (3.0f - 2.0f * x);
	}

	// Catmull-Romスプライン(uniform)。P1→P2間をtで補間する
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
}

// 初期化：パラメータ・テクスチャ・頂点バッファを用意する
bool SlashTrailInstance::Init(const SlashTrailParams& params, ITextureProvider* textureProvider)
{
	params_ = params;

	state_ = State::Idle;
	path_.clear();
	vertices_.clear();
	coreVertices_.clear();

	ResolveTexture(textureProvider);

	// ノイズが無い(テクスチャ未解決)ときはflags=0にして、線形フェードで消す
	noiseParams_ = ParticleNoiseParams{};
	noiseParams_.flags = noiseTexture_ ? ParticleNoiseFlag_Dissolve : 0;
	noiseParams_.noiseScale = 1.0f;	// タイリングは長さ/幅で別々に頂点のノイズUVへ反映する
	noiseParams_.scroll = { params_.Dissolve.ScrollU, params_.Dissolve.ScrollV };
	noiseParams_.dissolveStart = 0.0f;
	noiseParams_.dissolveSoftness = params_.Dissolve.Softness;
	noiseParams_.edgeWidth = params_.Dissolve.EdgeWidth;
	noiseParams_.edgeColor = { params_.Dissolve.EdgeColor.x, params_.Dissolve.EdgeColor.y, params_.Dissolve.EdgeColor.z, params_.Dissolve.EdgeIntensity };

	// 未確定の先頭点が1つ増えるぶんを見込んで頂点容量を確保する
	const UINT capacityPoints = std::max(params_.Spline.MaxPoints, params_.Record.MaxSamples + 1);

	renderer_ = std::make_shared<SlashTrailRenderer>();
	if (!renderer_->Init(capacityPoints * 2))
	{
		renderer_.reset();
		return false;
	}

	return true;
}

// 記録中として使い始める：前回の経路・頂点を破棄する(頂点バッファは使い回す)
void SlashTrailInstance::BeginLive()
{
	state_ = State::Recording;

	path_.clear();
	vertices_.clear();
	coreVertices_.clear();

	dissolveElapsed_ = 0.0f;
	liveIdleTime_ = 0.0f;

	static uint32_t s_counter = 0;
	++s_counter;
	noiseOffset_ = { HashOffset(s_counter * 2u), HashOffset(s_counter * 2u + 1u) };
}

// 記録中の経路を受け取る(未確定の先頭点つき)
void SlashTrailInstance::SetLivePath(const SlashTrailRecorder& recorder)
{
	if (state_ != State::Recording) { return; }

	recorder.BuildPath(path_);
	liveIdleTime_ = 0.0f;
}

// 確定した経路を受け取って消去へ移る
void SlashTrailInstance::Finish(const SlashTrailRecorder& recorder)
{
	if (state_ != State::Recording) { return; }

	recorder.BuildPath(path_);
	StartDissolve();
}

void SlashTrailInstance::StartDissolve()
{
	state_ = State::Dissolving;
	dissolveElapsed_ = 0.0f;
}

// 消去タイマーを進めて頂点を作り直す
void SlashTrailInstance::Update(float deltaTime)
{
	if (state_ == State::Idle) { return; }

	if (state_ == State::Recording)
	{
		liveIdleTime_ += deltaTime;
		if (liveIdleTime_ >= kLiveTimeout) { StartDissolve(); }
	}

	if (state_ == State::Dissolving) { dissolveElapsed_ += deltaTime; }

	if (IsFinished())
	{
		vertices_.clear();
		coreVertices_.clear();
	}
	else
	{
		RebuildVertices();
	}

	// 頂点バッファへの書き込みは1フレームにここで1回だけ行う
	if (renderer_) { renderer_->Upload(vertices_, coreVertices_); }
}

bool SlashTrailInstance::IsFinished() const
{
	if (state_ == State::Idle) { return true; }

	// 描画できる軌跡(2点以上)が残らなかった場合は即終了
	if (state_ == State::Dissolving)
	{
		return path_.size() < 2 || dissolveElapsed_ >= params_.Dissolve.Time;
	}

	return false;
}

// 描画：墨レイヤー→芯レイヤーの順に、同じrenderer_を使い回して2回描画する
void SlashTrailInstance::Draw(ParticleDrawPass pass) const
{
	if (!renderer_) { return; }

	if (KdHasDrawPassFlag(params_.Ink.DrawPassFlags, pass))
	{
		renderer_->Draw(SlashTrailRenderer::Layer::Ink, params_.Ink.BlendMode, texture_, noiseParams_, noiseTexture_);
	}

	if (params_.Core.Enabled && KdHasDrawPassFlag(params_.Core.DrawPassFlags, pass))
	{
		renderer_->Draw(SlashTrailRenderer::Layer::Core, params_.Core.BlendMode, texture_, noiseParams_, noiseTexture_);
	}
}

// path_から、墨レイヤー(vertices_)と芯レイヤー(coreVertices_)の頂点配列を作り直す
void SlashTrailInstance::RebuildVertices()
{
	vertices_.clear();
	coreVertices_.clear();

	using namespace DirectX::SimpleMath;

	if (path_.size() < 2) { return; }

	// ----- 1. スプラインによる細分化 -----
	resampled_.clear();

	if (params_.Spline.Enabled)
	{
		const size_t pathCount = path_.size();
		const size_t segmentCount = pathCount - 1;

		UINT subdiv = std::max(params_.Spline.Subdivisions, 1u);
		if (params_.Spline.MaxPoints > 1)
		{
			const UINT maxSubdiv = static_cast<UINT>((params_.Spline.MaxPoints - 1) / segmentCount);
			subdiv = std::clamp(subdiv, 1u, std::max(maxSubdiv, 1u));
		}

		resampled_.reserve(segmentCount * subdiv + 1);

		for (size_t i = 0; i < segmentCount; ++i)
		{
			const SlashTrailSample& s0 = path_[(i == 0) ? 0 : i - 1];
			const SlashTrailSample& s1 = path_[i];
			const SlashTrailSample& s2 = path_[i + 1];
			const SlashTrailSample& s3 = path_[(i + 2 < pathCount) ? i + 2 : pathCount - 1];

			// 区間の終点は次の区間の始点と重複するので、最終区間以外は含めない
			const UINT stepsThisSegment = (i + 1 == segmentCount) ? subdiv : subdiv - 1;

			for (UINT step = 0; step <= stepsThisSegment; ++step)
			{
				const float t = static_cast<float>(step) / static_cast<float>(subdiv);

				SlashTrailSample p;
				p.Tip = CatmullRom(s0.Tip, s1.Tip, s2.Tip, s3.Tip, t);
				p.Base = CatmullRom(s0.Base, s1.Base, s2.Base, s3.Base, t);
				p.Dist = std::lerp(s1.Dist, s2.Dist, t);
				p.Speed = std::lerp(s1.Speed, s2.Speed, t);
				resampled_.push_back(p);
			}
		}
	}
	else
	{
		resampled_ = path_;
	}

	const size_t pointCount = resampled_.size();
	if (pointCount < 2) { return; }

	vertices_.reserve(pointCount * 2);
	coreVertices_.reserve(pointCount * 2);

	// ----- 2. 頂点生成 -----
	const float strokeLengthSafe = (params_.Stroke.Length > 0.0f) ? params_.Stroke.Length : 0.0001f;
	const float speedRefSafe = (params_.Width.SpeedReference > 0.0f) ? params_.Width.SpeedReference : 0.0001f;
	const float dissolveTimeSafe = (params_.Dissolve.Time > 0.0f) ? params_.Dissolve.Time : 0.0001f;

	const float totalLength = resampled_.back().Dist;

	// 一括消去：全頂点に共通のLifeRate(1→0)。PS側のノイズ(ノイズ無しなら線形フェード)が消す
	const float lifeRate = (state_ == State::Dissolving)
		? 1.0f - Smoothstep01(dissolveElapsed_ / dissolveTimeSafe)
		: 1.0f;

	const Vector3 camPos = KdShaderManager::Instance().GetCameraCB().CamPos;

	// 幅方向が決まらない点(接線と視線が平行)で、前の点の向きを引き継ぐ用
	Vector3 prevWidthDir = Vector3::Zero;
	bool hasPrevWidthDir = false;

	for (size_t idx = 0; idx < pointCount; ++idx)
	{
		const SlashTrailSample& sample = resampled_[idx];
		// 始点からの距離で決まるので、記録済みの点のuは後から変わらない
		const float u = std::clamp(sample.Dist / strokeLengthSafe, 0.0f, 1.0f);

		// 先頭(totalLength)に近いほど幅を絞る
		const float headFactor = (params_.Stroke.HeadTaperLength > 1e-5f)
			? Smoothstep01((totalLength - sample.Dist) / params_.Stroke.HeadTaperLength)
			: 1.0f;

		const Vector3 color = Vector3::Lerp(params_.Ink.ColdColor, params_.Ink.HotColor, u);

		const float speedScale = std::clamp(
			sample.Speed / speedRefSafe,
			params_.Width.MinScale, params_.Width.MaxScale);

		const Vector3 center = (sample.Tip + sample.Base) * 0.5f;

		// ----- カメラ正対(幅方向=接線×視線、長さ=Tip-Base間の距離) -----
		const float widthLen = (sample.Tip - sample.Base).Length();
		const Vector3 widthVec = sample.Tip - sample.Base;

		const Vector3 tangent = resampled_[std::min(idx + 1, pointCount - 1)].Tip - resampled_[(idx > 0) ? idx - 1 : 0].Tip;
		const Vector3 viewDir = camPos - center;

		// 向きは符号を反転させず固定する(進行方向に対して常に同じ側がv=0になり、カメラを動かしても裏返らない)
		Vector3 widthDir = prevWidthDir;
		Vector3 billboardDir = tangent.Cross(viewDir);
		if (billboardDir.LengthSquared() > 1e-10f)
		{
			billboardDir.Normalize();
			widthDir = billboardDir;
		}
		else if (!hasPrevWidthDir && widthLen > 1e-5f)
		{
			widthDir = widthVec / widthLen;
		}

		prevWidthDir = widthDir;
		hasPrevWidthDir = (widthDir.LengthSquared() > 1e-10f);

		Vector3 correctedWidthVec = widthDir * widthLen;
		correctedWidthVec *= speedScale * headFactor;

		const Vector3 halfWidth = correctedWidthVec * 0.5f;

		// ノイズUV(タイル単位)：uは始点からの距離、vは(0/1)×Tip-Base間の距離に、長さ/幅別の倍率を掛ける。
		// 幅の変化(速度・先端の絞り)を含めないので、記録済みの点の模様は後から動かない
		const float noiseU = sample.Dist * params_.Dissolve.NoiseScaleU + noiseOffset_.x;
		const float noiseVBase = widthLen * params_.Dissolve.NoiseScaleV + noiseOffset_.y;

		SlashTrailVertex vTip;
		vTip.Position = center + halfWidth;
		vTip.UV = { u, 0.0f };
		vTip.Color = { color.x, color.y, color.z, 1.0f };
		vTip.Extra = { lifeRate, noiseU, noiseOffset_.y, 0.0f };
		vertices_.push_back(vTip);

		SlashTrailVertex vBase;
		vBase.Position = center - halfWidth;
		vBase.UV = { u, 1.0f };
		vBase.Color = { color.x, color.y, color.z, 1.0f };
		vBase.Extra = { lifeRate, noiseU, noiseVBase, 0.0f };
		vertices_.push_back(vBase);

		if (params_.Core.Enabled)
		{
			const Vector3 coreHalfWidth = halfWidth * params_.Core.WidthScale;
			const float coreAlpha = std::lerp(params_.Core.TailAlpha, 1.0f, u);

			SlashTrailVertex vTipCore;
			vTipCore.Position = center + coreHalfWidth;
			vTipCore.UV = { u, 0.0f };
			vTipCore.Color = { params_.Core.Color.x, params_.Core.Color.y, params_.Core.Color.z, coreAlpha };
			vTipCore.Extra = { lifeRate, noiseU, noiseOffset_.y, 0.0f };
			coreVertices_.push_back(vTipCore);

			SlashTrailVertex vBaseCore;
			vBaseCore.Position = center - coreHalfWidth;
			vBaseCore.UV = { u, 1.0f };
			vBaseCore.Color = { params_.Core.Color.x, params_.Core.Color.y, params_.Core.Color.z, coreAlpha };
			vBaseCore.Extra = { lifeRate, noiseU, noiseVBase, 0.0f };
			coreVertices_.push_back(vBaseCore);
		}
	}
}

// テクスチャを解決する(未指定・解決失敗ならnullptr。墨の帯は白テクスチャ、消去は線形フェードになる)
void SlashTrailInstance::ResolveTexture(ITextureProvider* textureProvider)
{
	texture_.reset();
	noiseTexture_.reset();

	if (!textureProvider) { return; }

	if (!params_.TexturePath.empty())
	{
		texture_ = textureProvider->GetTexture(params_.TexturePath);
	}

	if (!params_.Dissolve.NoiseTexturePath.empty())
	{
		noiseTexture_ = textureProvider->GetTexture(params_.Dissolve.NoiseTexturePath);
	}
}
