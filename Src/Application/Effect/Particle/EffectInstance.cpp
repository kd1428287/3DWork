#include "Application/main.h"
#include "EffectInstance.h"
#include <algorithm>
#include <random>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 生成
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectInstance::Init(const GPUParticleParams& params, ITextureProvider* textureProvider)
{
	params_ = params;

	particle_ = std::make_shared<ParticleBuffer>();
	if (!particle_->Init(params_.MaxParticleNum))
	{
		assert(0 && "EffectInstance：ParticleBuffer初期化失敗");
		particle_.reset();
		capacity_ = 0;
		return false;
	}
	capacity_ = params_.MaxParticleNum;

	isPlaying_ = false;
	burstTimer_ = 0.0f;
	aliveTimer_ = 0.0f;
	continuousAccum_.assign(params_.Layers.size(), 0.0f);

	staggered_.clear();
	staggerDuration_ = 0.0f;
	staggerElapsed_ = 0.0f;

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

	// 未初期化、またはMaxParticleNumが変わった場合はParticleBufferごと作り直す
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

	// 全粒子が死んでいる間は更新を省く。尽きた瞬間に書き込み位置を先頭へ戻す
	const bool wasAlive = aliveTimer_ > 0.0f;
	aliveTimer_ = std::max(0.0f, aliveTimer_ - deltaTime);
	if (aliveTimer_ > 0.0f)
	{
		particle_->Update(deltaTime, params_.Gravity, pendingFollowDelta_);
	}
	else if (wasAlive)
	{
		particle_->Rewind();
	}
	pendingFollowDelta_ = { 0,0,0 };

	if (staggerDuration_ > 0.0f) { EmitStaggered(deltaTime, worldPos, baseDir); }

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
				EmitInternal(layer.ToEmitParameter(worldPos, baseDir), emitNum);
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

		EmitInternal(layer.ToEmitParameter(worldPos, baseDir), (UINT)layer.Count);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 単発発生(追従エフェクト用)：全Layersを1回だけ発生させ、最長の粒子寿命を返す
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
float EffectInstance::EmitOnce(const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir, float convergeDuration, float spawnSpread)
{
	if (!particle_) { return 0.0f; }

	staggered_.clear();
	staggerDuration_ = 0.0f;
	staggerElapsed_ = 0.0f;

	// 最後の粒子にも最低限の移動時間を残す
	const float spread = std::clamp(spawnSpread, 0.0f, 0.95f);

	float maxLife = 0.0f;
	for (size_t i = 0; i < params_.Layers.size(); ++i)
	{
		const auto& layer = params_.Layers[i];
		if (layer.Count <= 0) { continue; }

		ParticleBuffer::EmitParameter param = layer.ToEmitParameter(worldPos, baseDir);

		if (convergeDuration > 0.0f && param.Distribution == ParticleEmitDistribution::Converge)
		{
			// ばらけ発生：各粒子の発生時刻を抽選し、Update()側で予定時刻に発生させる
			if (spread > 0.0f)
			{
				static thread_local std::mt19937 rng{ std::random_device{}() };
				std::uniform_real_distribution<float> dist(0.0f, convergeDuration * spread);

				StaggeredLayer staggered;
				staggered.layerIndex = i;
				staggered.times.resize((size_t)layer.Count);
				for (float& t : staggered.times) { t = dist(rng); }
				std::sort(staggered.times.begin(), staggered.times.end());

				staggered_.push_back(std::move(staggered));
				staggerDuration_ = convergeDuration;
				maxLife = std::max(maxLife, convergeDuration);
				continue;
			}

			// 一括発生：寿命=収束時間にし、速度は寿命から自動計算させる(ConvergeSpeed=0)
			param.LifeMin = convergeDuration;
			param.LifeMax = convergeDuration;
			param.ConvergeSpeed = 0.0f;
		}

		maxLife = std::max(maxLife, param.LifeMax);
		EmitInternal(param, (UINT)layer.Count);
	}
	return maxLife;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ばらけ発生：予定時刻に達した粒子を、残り時間を寿命として発生させる(全粒子が同時刻に収束する)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::EmitStaggered(float deltaTime, const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir)
{
	staggerElapsed_ += deltaTime;
	const float remaining = std::max(staggerDuration_ - staggerElapsed_, 0.001f);

	for (auto& s : staggered_)
	{
		UINT num = 0;
		while (s.cursor < s.times.size() && s.times[s.cursor] <= staggerElapsed_)
		{
			++s.cursor;
			++num;
		}
		if (num == 0) { continue; }

		ParticleBuffer::EmitParameter param = params_.Layers[s.layerIndex].ToEmitParameter(worldPos, baseDir);
		param.LifeMin = remaining;	// 残り時間が寿命：発生が遅い粒子ほど速く動く
		param.LifeMax = remaining;
		param.ConvergeSpeed = 0.0f;
		EmitInternal(param, num);
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

void EffectInstance::StopEmission()
{
	isPlaying_ = false;
	staggered_.clear();
	staggerDuration_ = 0.0f;
}

void EffectInstance::Recycle()
{
	isPlaying_ = false;
	burstTimer_ = 0.0f;
	staggered_.clear();
	staggerDuration_ = 0.0f;
	staggerElapsed_ = 0.0f;
	pendingFollowDelta_ = { 0,0,0 };

	if (particle_)
	{
		// 残っている粒子があれば消す(使い回し先に幽霊を残さない)
		if (aliveTimer_ > 0.0f) { particle_->Clear(); }
		else { particle_->Rewind(); }
	}
	aliveTimer_ = 0.0f;
}

void EffectInstance::EmitInternal(const ParticleBuffer::EmitParameter& param, UINT count)
{
	particle_->Emit(param, count);
	aliveTimer_ = std::max(aliveTimer_, param.LifeMax + kAliveMargin);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画：テクスチャ・BlendMode込み
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::Draw() const
{
	if (!particle_ || !IsActive()) { return; }

	particle_->Draw(texture_, params_.BlendMode);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画：DrawPassが一致する時だけ描画する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::Draw(ParticleDrawPass pass) const
{
	if (IsLiquid()) { return; }	// 液体はDrawLiquid()側でまとめて描画する

	if (!KdHasDrawPassFlag(params_.DrawPassFlags, pass)) { return; }

	Draw();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 液体の密度描画：Addで密度RTへ描く(色ではなくR値が密度になる)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::DrawLiquid() const
{
	if (!particle_ || !IsLiquid() || !IsActive()) { return; }

	particle_->Draw(texture_, params_.BlendMode);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// テクスチャ解決
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectInstance::ResolveTexture(ITextureProvider* textureProvider)
{
	texture_.reset();

	if (!textureProvider) { return; }
	if (params_.TexturePath.empty()) { return; }

	texture_ = textureProvider->GetTexture(params_.TexturePath);
}