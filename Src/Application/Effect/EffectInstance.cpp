#include "../main.h"
#include "EffectInstance.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 生成
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectInstance::Init(const GPUParticleParams& params, ITextureProvider* textureProvider)
{
	m_params = params;

	m_particle = std::make_shared<KdGPUParticle>();
	if (!m_particle->Init(m_params.MaxParticleNum))
	{
		assert(0 && "EffectInstance：KdGPUParticle初期化失敗");
		m_particle.reset();
		m_capacity = 0;
		return false;
	}
	m_capacity = m_params.MaxParticleNum;

	m_isPlaying = false;
	m_burstTimer = 0.0f;
	m_continuousAccum.assign(m_params.Layers.size(), 0.0f);

	ResolveTexture(textureProvider);

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// パラメータ差し替え(必要な部分だけ作り直す)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectInstance::Reconfigure(const GPUParticleParams& params, ITextureProvider* textureProvider)
{
	const bool capacityChanged = (params.MaxParticleNum != m_capacity);
	const bool textureChanged = (params.TexturePath != m_params.TexturePath);

	m_params = params;

	// 未初期化、またはMaxParticleNumが変わった場合はKdGPUParticleごと作り直す
	if (!m_particle || capacityChanged)
	{
		return Init(m_params, textureProvider);
	}

	// テクスチャパスだけ変わった場合は解決だけやり直す(パーティクルバッファは維持)
	if (textureChanged)
	{
		ResolveTexture(textureProvider);
	}

	// レイヤー数が変わっている可能性があるため、端数蓄積バッファをサイズ合わせ
	m_continuousAccum.assign(m_params.Layers.size(), 0.0f);

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 更新：Gravity適用 + (再生中なら)自動発生の処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::Update(float deltaTime, const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir)
{
	if (!m_particle) { return; }

	m_particle->Update(deltaTime, m_params.Gravity);

	if (!m_isPlaying) { return; }

	if (m_params.EmitMode == KdParticleEmitMode::Continuous)
	{
		// レイヤーごとに「Count個/秒」のペースで端数を蓄積し、1個分たまったら発生
		if (m_continuousAccum.size() != m_params.Layers.size())
		{
			m_continuousAccum.assign(m_params.Layers.size(), 0.0f);
		}

		for (size_t i = 0; i < m_params.Layers.size(); ++i)
		{
			const GPUParticleLayer& layer = m_params.Layers[i];
			if (layer.Count <= 0) { continue; }

			m_continuousAccum[i] += (float)layer.Count * deltaTime;

			const UINT emitNum = (UINT)m_continuousAccum[i];
			if (emitNum > 0)
			{
				m_continuousAccum[i] -= (float)emitNum;
				m_particle->Emit(layer.Shape.ToEmitParameter(worldPos, baseDir), emitNum);
			}
		}
	}
	else // Burst：EmitIntervalごとに全Layersをまとめて再発生
	{
		if (m_params.EmitInterval <= 0.0f) { return; }	// 単発扱いのためPlay()以降は何もしない

		m_burstTimer += deltaTime;
		if (m_burstTimer >= m_params.EmitInterval)
		{
			m_burstTimer -= m_params.EmitInterval;
			Emit(worldPos, baseDir);
		}
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 単発発生：全Layersぶんをまとめて発生させる(各LayerのCount個ずつ)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::Emit(const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir)
{
	if (!m_particle) { return; }

	for (const auto& layer : m_params.Layers)
	{
		if (layer.Count <= 0) { continue; }

		m_particle->Emit(layer.Shape.ToEmitParameter(worldPos, baseDir), (UINT)layer.Count);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 再生開始
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::Play(const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir)
{
	if (!m_particle) { return; }

	// Burstかつ単発(EmitInterval<=0)の場合：即1回発生して終わり。再生状態には入らない
	if (m_params.EmitMode == KdParticleEmitMode::Burst && m_params.EmitInterval <= 0.0f)
	{
		Emit(worldPos, baseDir);
		return;
	}

	m_isPlaying = true;
	m_burstTimer = 0.0f;
	m_continuousAccum.assign(m_params.Layers.size(), 0.0f);

	// Burstリピートの場合は再生開始と同時に1回目を発生させておく
	if (m_params.EmitMode == KdParticleEmitMode::Burst)
	{
		Emit(worldPos, baseDir);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 新規発生を止める(既存パーティクルの寿命消化・描画は継続される)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::Stop()
{
	m_isPlaying = false;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画：テクスチャ込み
//	※BlendModeはKdGPUParticle::Draw()側が対応するまでは常に加算合成で描画される。
//	  対応が入ったタイミングでここにBlendModeを渡す処理を追加すること。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::Draw() const
{
	if (!m_particle) { return; }

	m_particle->Draw(m_texture);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// テクスチャ解決
//	textureProviderがnullptr、またはTexturePathが空の場合はテクスチャ無し(nullptr)のまま
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::ResolveTexture(ITextureProvider* textureProvider)
{
	m_texture.reset();

	if (!textureProvider) { return; }
	if (m_params.TexturePath.empty()) { return; }

	m_texture = textureProvider->GetTexture(m_params.TexturePath);
}