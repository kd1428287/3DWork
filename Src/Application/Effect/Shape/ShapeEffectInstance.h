#pragma once

#include <memory>
#include <string>

#include "ShapeEffectMotion.h"
#include "../Common/ITextureProvider.h"
#include "Framework/Shader/GPUParticle/ParticleBuffer.h"	// ParticleBlendMode
#include "Application/Definitions/Effect/EffectParams.h"	// ParticleDrawPass

struct ShapeEffectParams
{
	std::string			TexturePath;
	ParticleBlendMode	BlendMode = ParticleBlendMode::Alpha;
	ParticleDrawPass	DrawPassFlags = ParticleDrawPass::Default;	// 描画されるパス(ビットフラグ)
	Math::Vector4		Color = { 1, 1, 1, 1 };	// テクスチャへ乗算(aは動きのalphaと掛け合わされる)
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 一枚テクスチャエフェクト1個分：経過時間と動きを持ち、基準点を受け取って描画する
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 基準点は保持せず、Draw()のたびに渡す(追従先の破棄で壊れない)。
// 固定したい場合は、呼び出し側が固定した行列を渡し続ける。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class ShapeEffectInstance
{
public:

	// motionはステートレスなので、複数インスタンスで共有してよい
	void Init(std::shared_ptr<IEffectMotion> motion, const ShapeEffectParams& params, ITextureProvider* textureProvider);

	void Update(float deltaTime) { time_ += deltaTime; }

	// 経過時間を直接指定する(エディタの再生バーでのシーク用)
	void SetTime(float time) { time_ = time; }
	float GetTime() const { return time_; }

	// 動きの最後まで再生したか
	bool IsFinished() const { return !motion_ || time_ >= motion_->GetDuration(); }

	// baseは回転+平行移動の基準点行列
	void Draw(const Math::Matrix& base) const;

	const ShapeEffectParams& GetParams() const { return params_; }

private:

	ShapeEffectParams				params_;
	std::shared_ptr<IEffectMotion>	motion_;
	std::shared_ptr<KdTexture>		texture_;
	float							time_ = 0.0f;
};
