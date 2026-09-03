#pragma once

#include "EffectParams.h"
#include "ITextureProvider.h"
#include "../../Framework/Shader/GPUParticle/KdGPUParticle.h"

#include <memory>
#include <vector>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エフェクト1個分の実行時状態と、更新/発生/描画ロジックを一元管理するクラス
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 【対象範囲】
//   座標だけで足りる単純エフェクト(GPUParticleParams、HitSpark/FootDust/BloodSplatter等)が対象。
//   鍔迫り合いの火花(WeaponClashEffectParams / DirectionalSparkLayer)は今回のリファクタ範囲外。
//   ただし形状定義(DirectionalEmitShape)はEffectParams.h側で両者が共用している。
//
// 【使い方(想定)】
//   EffectInstance instance;
//   instance.Init(params, textureProvider);
//     :
//   // 単発エフェクト(HitSpark等)：都度Emitを直接呼ぶ
//   instance.Emit(worldPos, baseDir);
//     :
//   // 継続/リピートエフェクト(松明の火の粉、足元の砂煙等)
//   instance.Play(worldPos, baseDir);          // 再生開始
//   // 毎フレーム
//   instance.Update(deltaTime, worldPos, baseDir); // 発生位置・方向が動く場合は毎回渡す
//   instance.Draw();
//     :
//   instance.Stop();                            // 新規発生を止める(既存粒子はそのまま消化)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class EffectInstance
{
public:

	EffectInstance() {}
	~EffectInstance() {}

	// コピー禁止
	EffectInstance(const EffectInstance&) = delete;
	EffectInstance& operator=(const EffectInstance&) = delete;

	// ムーブは許可
	EffectInstance(EffectInstance&&) = default;
	EffectInstance& operator=(EffectInstance&&) = default;

	bool Init(const GPUParticleParams& params, ITextureProvider* textureProvider);
	bool Reconfigure(const GPUParticleParams& params, ITextureProvider* textureProvider);

	// 毎フレーム呼ぶ：重力適用 + (再生中なら)自動発生の処理
	//	worldPos/baseDir：継続発生・Burstリピート時の発生位置/方向(動く発生源はここで毎回更新)
	void Update(float deltaTime, const DirectX::SimpleMath::Vector3& worldPos = { 0,0,0 }, const DirectX::SimpleMath::Vector3& baseDir = { 0,0,0 });

	// 単発発生：全Layersぶんをまとめて発生させる(各LayerのCount個ずつ)
	//	Burst/Continuousどちらのモードでも「今すぐ1回」出したい場合に使える
	void Emit(const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir = { 0,0,0 });

	// 再生開始
	//	Burst かつ EmitInterval<=0(単発)の場合：即座に1回Emitして終了(IsPlaying()はfalseのまま、Stop()不要)
	//	それ以外(Burstリピート／Continuous)の場合：m_isPlaying=trueになり、以後Update()のたびに自動発生する
	void Play(const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir = { 0,0,0 });

	// 新規発生を止める(既存パーティクルの寿命消化・描画は継続される)
	void Stop();

	bool IsPlaying() const { return m_isPlaying; }

	void Draw() const;
	const GPUParticleParams& GetParams() const { return m_params; }

	bool IsInitialized() const { return m_particle != nullptr; }

private:

	void ResolveTexture(ITextureProvider* textureProvider);

	GPUParticleParams				m_params;
	std::shared_ptr<KdGPUParticle>	m_particle;
	std::shared_ptr<KdTexture>		m_texture;

	UINT	m_capacity = 0;

	//================================================
	// 再生状態(Continuous／Burstリピート用)
	//================================================
	bool	m_isPlaying = false;

	float	m_burstTimer = 0.0f;			// Burst：次発生までの経過時間

	// Continuous：Layerごとの発生数の端数を蓄積(deltaTime * Count/秒ぶんを毎フレーム加算)
	//	1.0を超えた分だけ整数個発生させ、余りは次フレームへ繰り越す
	std::vector<float>	m_continuousAccum;
};