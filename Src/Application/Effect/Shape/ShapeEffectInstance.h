#pragma once

#include "ShapeEffectMotion.h"
#include "../Common/ITextureProvider.h"
#include "Framework/Shader/GPUParticle/ParticleBuffer.h"	// ParticleBlendMode
#include "Application/Definitions/Effect/EffectParams.h"	// ParticleDrawPass

// ビルボード手法
enum class ShapeBillboardMode
{
	None,	// 向きを固定(基準点の回転に追従する)
	Full,	// 常にカメラへ正対する(Rot Zのロールだけ有効)
	YAxis,	// Y軸回りだけカメラへ向く(立ったまま回り込む)
	Count,	// 個数(名前表・エディタのコンボ用。末尾に保つ)
};

// JSON保存・エディタ表示用の名前
inline const char* BillboardModeName(ShapeBillboardMode mode)
{
	static const char* const kNames[] = { "None", "Full", "YAxis" };
	static_assert(sizeof(kNames) / sizeof(kNames[0]) == (size_t)ShapeBillboardMode::Count, "ShapeBillboardModeの名前表と個数を合わせる事");

	const size_t i = (size_t)mode;
	return (i < (size_t)ShapeBillboardMode::Count) ? kNames[i] : "None";
}

// 未知の名前はNoneになる
inline ShapeBillboardMode BillboardModeFromName(const std::string& name)
{
	for (size_t i = 0; i < (size_t)ShapeBillboardMode::Count; ++i)
	{
		if (name == BillboardModeName((ShapeBillboardMode)i)) { return (ShapeBillboardMode)i; }
	}
	return ShapeBillboardMode::None;
}

struct ShapeEffectParams
{
	std::string			TexturePath;
	ParticleBlendMode	BlendMode = ParticleBlendMode::Alpha;
	ParticleDrawPass	DrawPassFlags = ParticleDrawPass::Default;	// 描画されるパス(ビットフラグ)
	Math::Vector4		Color = { 1, 1, 1, 1 };	// テクスチャへ乗算(aは動きのalphaと掛け合わされる)

	bool				DepthTest = true;		// falseなら手前の物に隠されず常に描く

	std::string			NoiseTexturePath;		// ノイズ用テクスチャ(タイリング可能な画像のRチャンネル)
	ParticleNoiseParams	Noise;					// flags=0ならノイズ無効。ScrollはUV/秒(パーティクルと違い寿命ではなく時間で流れる)
};

// 1エフェクト内の1枚ぶんの配置と動き(テクスチャ・ブレンド・ノイズ等の素材はエフェクト内で共通)
struct ShapePart
{
	std::string				Name;
	float					Delay = 0.0f;		// エフェクト開始からの待ち時間(秒)
	ShapeBillboardMode		Billboard = ShapeBillboardMode::None;
	float					DepthOffset = 0.0f;	// カメラ方向への深度のずらし量(ワールド単位。+で手前。画面上の位置・大きさは変えない)

	std::vector<EffectKey>	Keys;				// 動き(キー列)
	std::shared_ptr<IEffectMotion>	Motion;		// 指定があればKeysの代わりに使う(共有や手続き型の動き用)
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 一枚テクスチャエフェクト1個分：経過時間と、複数パーツの動きを持ち、基準点を受け取って描画する
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 基準点は保持せず、Draw()のたびに渡す(追従先の破棄で壊れない)。
// 固定したい場合は、呼び出し側が固定した行列を渡し続ける。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class ShapeEffectInstance
{
public:

	// partsのMotionが空のものはKeysから動きを作る(Motionはステートレスなので、複数インスタンスで共有してよい)
	void Init(const ShapeEffectParams& params, const std::vector<ShapePart>& parts, ITextureProvider* textureProvider);

	void Update(float deltaTime) { time_ += deltaTime; }

	// 経過時間を直接指定する(エディタの再生バーでのシーク用)
	void SetTime(float time) { time_ = time; }
	float GetTime() const { return time_; }

	// 全パーツの動きが終わるまでの秒数(各パーツのDelayを含む)
	float GetDuration() const { return duration_; }

	// 全パーツの動きの最後まで再生したか
	bool IsFinished() const { return time_ >= duration_; }

	// baseは回転+平行移動の基準点行列
	void Draw(const Math::Matrix& base) const;

	const ShapeEffectParams& GetParams() const { return params_; }

private:

	ShapeEffectParams				params_;
	std::vector<ShapePart>			parts_;
	std::shared_ptr<KdTexture>		texture_;
	std::shared_ptr<KdTexture>		noiseTexture_;
	float							time_ = 0.0f;
	float							duration_ = 0.0f;
};
