#pragma once

#include "Application/Definitions/Effect/EffectParams.h"
#include "../Common/ITextureProvider.h"
#include "Framework/Shader/GPUParticle/ParticleBuffer.h"

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

	// 単発発生(追従エフェクト用)：全Layersを1回だけ発生させ、最も長い粒子寿命(秒)を返す
	//	convergeDuration>0なら、Converge層がちょうどその時刻に収束しきるようにする
	//	spawnSpread>0なら、Converge層の発生を収束時間のその割合へばらけさせる(全粒子の収束時刻は同じ)
	float EmitOnce(const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir = { 0,0,0 }, float convergeDuration = 0.0f, float spawnSpread = 0.0f);

	// 次のUpdate()で、生きている全粒子を平行移動する量を足す(発生源への追従用)
	void AddFollowDelta(const DirectX::SimpleMath::Vector3& delta) { pendingFollowDelta_ += delta; }

	// 再生開始
	//	Burst かつ EmitInterval<=0(単発)の場合：即座に1回Emitして終了(IsPlaying()はfalseのまま、Stop()不要)
	//	それ以外(Burstリピート／Continuous)の場合：isPlaying_=trueになり、以後Update()のたびに自動発生する
	void Play(const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir = { 0,0,0 });

	// 新規発生を止める(既存パーティクルの寿命消化・描画は継続される)
	void Stop();

	// Stop()に加え、ばらけ発生の未発生分も取り消す(残りの粒子は自然に消える)
	void StopEmission();

	// 生きている粒子がありうるか(false=更新・描画を省ける)
	bool IsActive() const { return aliveTimer_ > 0.0f; }

	// 最後の粒子が消えるまでの残り秒数(目安)
	float GetRemainingLife() const { return aliveTimer_; }

	// 使い回し前の後始末：発生を止め、残っている粒子があれば消す
	void Recycle();

	bool IsPlaying() const { return isPlaying_; }

	// 無条件に描画する(プレビュー単体ビューポート等、DrawPassによる分岐が不要な呼び出し元向け)
	void Draw() const;

	// params.DrawPassFlagsにpassのビットが含まれる時だけ描画する
	//	メインシーンのDrawLit/DrawBloomの両方から、それぞれ対応するpassで毎フレーム
	//	呼んでもらう想定(1つのエフェクトが両方のフラグを持てば、両方から描画される)
	void Draw(ParticleDrawPass pass) const;

	// 液体表現(BlendMode=LiquidInk)かどうか。trueのものはDraw(pass)では描画されない
	bool IsLiquid() const { return params_.BlendMode == ParticleBlendMode::LiquidInk; }

	// 液体表現の密度描画：EffectDispatcher::DrawLiquid()のBeginLiquid〜EndLiquidの間から呼ぶ
	void DrawLiquid() const;

	const GPUParticleParams& GetParams() const { return params_; }

	bool IsInitialized() const { return particle_ != nullptr; }

private:

	void ResolveTexture(ITextureProvider* textureProvider);

	// ParticleBuffer::Emit()の代わりに使う(最後の粒子が消える時刻を記録する)
	void EmitInternal(const ParticleBuffer::EmitParameter& param, UINT count);

	static constexpr float kAliveMargin = 0.1f;	// 寿命の見積もりに足す余白(秒)

	// ばらけ発生中のConverge層を、予定時刻に達した分だけ発生させる(Update()から呼ぶ)
	void EmitStaggered(float deltaTime, const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir);

	GPUParticleParams				params_;
	std::shared_ptr<ParticleBuffer>	particle_;
	std::shared_ptr<KdTexture>		texture_;

	UINT	capacity_ = 0;

	float	aliveTimer_ = 0.0f;	// 最後に発生した粒子が尽きるまでの残り秒数(0なら全て死亡)

	DirectX::SimpleMath::Vector3	pendingFollowDelta_ = { 0,0,0 };	// 次のUpdate()で粒子へ適用する平行移動量

	//================================================
	// ばらけ発生(EmitOnce()のspawnSpread>0)用
	//================================================
	struct StaggeredLayer
	{
		size_t				layerIndex = 0;
		std::vector<float>	times;		// 各粒子の発生予定時刻(昇順)
		size_t				cursor = 0;	// 次に発生させる粒子
	};
	std::vector<StaggeredLayer>	staggered_;
	float	staggerDuration_ = 0.0f;	// 収束しきるまでの時間(0なら無効)
	float	staggerElapsed_ = 0.0f;

	//================================================
	// 再生状態(Continuous／Burstリピート用)
	//================================================
	bool	isPlaying_ = false;

	float	burstTimer_ = 0.0f;			// Burst：次発生までの経過時間

	// Continuous：Layerごとの発生数の端数を蓄積(deltaTime * Count/秒ぶんを毎フレーム加算)
	//	1.0を超えた分だけ整数個発生させ、余りは次フレームへ繰り越す
	std::vector<float>	continuousAccum_;
};