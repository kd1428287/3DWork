#include "../main.h"
#include "EffectInstance.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 生成
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectInstance::Init(const GPUParticleParams& params, ITextureProvider* textureProvider)
{
	params_ = params;

	particle_ = std::make_shared<KdGPUParticle>();
	if (!particle_->Init(params_.MaxParticleNum))
	{
		assert(0 && "EffectInstance：KdGPUParticle初期化失敗");
		particle_.reset();
		capacity_ = 0;
		return false;
	}
	capacity_ = params_.MaxParticleNum;

	isPlaying_ = false;
	burstTimer_ = 0.0f;
	continuousAccum_.assign(params_.Layers.size(), 0.0f);

	ResolveTexture(textureProvider);

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// パラメータ差し替え(必要な部分だけ作り直す)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectInstance::Reconfigure(const GPUParticleParams& params, ITextureProvider* textureProvider)
{
	const bool capacityChanged = (params.MaxParticleNum != capacity_);
	const bool textureChanged = (params.TexturePath != params_.TexturePath);

	params_ = params;

	// 未初期化、またはMaxParticleNumが変わった場合はKdGPUParticleごと作り直す
	if (!particle_ || capacityChanged)
	{
		return Init(params_, textureProvider);
	}

	// テクスチャパスだけ変わった場合は解決だけやり直す(パーティクルバッファは維持)
	if (textureChanged)
	{
		ResolveTexture(textureProvider);
	}

	// レイヤー数が変わっている可能性があるため、端数蓄積バッファをサイズ合わせ
	continuousAccum_.assign(params_.Layers.size(), 0.0f);

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 更新：Gravity適用 + (再生中なら)自動発生の処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::Update(float deltaTime, const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir)
{
	if (!particle_) { return; }

	particle_->Update(deltaTime, params_.Gravity);

	if (!isPlaying_) { return; }

	if (params_.EmitMode == ParticleEmitMode::Continuous)
	{
		// レイヤーごとに「Count個/秒」のペースで端数を蓄積し、1個分たまったら発生
		if (continuousAccum_.size() != params_.Layers.size())
		{
			continuousAccum_.assign(params_.Layers.size(), 0.0f);
		}

		for (size_t i = 0; i < params_.Layers.size(); ++i)
		{
			const GPUParticleLayer& layer = params_.Layers[i];
			if (layer.Count <= 0) { continue; }

			continuousAccum_[i] += (float)layer.Count * deltaTime;

			const UINT emitNum = (UINT)continuousAccum_[i];
			if (emitNum > 0)
			{
				continuousAccum_[i] -= (float)emitNum;
				particle_->Emit(layer.ToEmitParameter(worldPos, baseDir), emitNum);
			}
		}
	}
	else // Burst：EmitIntervalごとに全Layersをまとめて再発生
	{
		if (params_.EmitInterval <= 0.0f) { return; }	// 単発扱いのためPlay()以降は何もしない

		burstTimer_ += deltaTime;
		if (burstTimer_ >= params_.EmitInterval)
		{
			burstTimer_ -= params_.EmitInterval;
			Emit(worldPos, baseDir);
		}
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 単発発生：全Layersぶんをまとめて発生させる(各LayerのCount個ずつ)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::Emit(const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir)
{
	if (!particle_) { return; }

	for (const auto& layer : params_.Layers)
	{
		if (layer.Count <= 0) { continue; }

		particle_->Emit(layer.ToEmitParameter(worldPos, baseDir), (UINT)layer.Count);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 再生開始
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::Play(const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir)
{
	if (!particle_) { return; }

	// Burstかつ単発(EmitInterval<=0)の場合：即1回発生して終わり。再生状態には入らない
	if (params_.EmitMode == ParticleEmitMode::Burst && params_.EmitInterval <= 0.0f)
	{
		Emit(worldPos, baseDir);
		return;
	}

	isPlaying_ = true;
	burstTimer_ = 0.0f;
	continuousAccum_.assign(params_.Layers.size(), 0.0f);

	// Burstリピートの場合は再生開始と同時に1回目を発生させておく
	if (params_.EmitMode == ParticleEmitMode::Burst)
	{
		Emit(worldPos, baseDir);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 新規発生を止める(既存パーティクルの寿命消化・描画は継続される)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::Stop()
{
	isPlaying_ = false;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画：テクスチャ・BlendMode込み
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::Draw() const
{
	if (!particle_) { return; }

	particle_->Draw(texture_, params_.BlendMode);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画：DrawPassが一致する時だけ描画する
//	(DrawLit/DrawBloomの両方から毎フレーム呼ばれる想定。一致しない方では何もしない)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::Draw(ParticleDrawPass pass) const
{
	if (!KdHasDrawPassFlag(params_.DrawPassFlags, pass)) { return; }

	Draw();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// テクスチャ解決
//	textureProviderがnullptr、またはTexturePathが空の場合はテクスチャ無し(nullptr)のまま
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::ResolveTexture(ITextureProvider* textureProvider)
{
	texture_.reset();

	if (!textureProvider) { return; }
	if (params_.TexturePath.empty()) { return; }

	texture_ = textureProvider->GetTexture(params_.TexturePath);
}