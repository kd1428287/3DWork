#pragma once

#include "../GPUParticle/ParticleBuffer.h"	// ParticleBlendMode を流用する為

// 1枚のクアッドを描くための入力
struct ShapeEffectDrawInfo
{
	std::shared_ptr<KdTexture>	Texture;
	ParticleBlendMode			BlendMode = ParticleBlendMode::Alpha;
	Math::Matrix				World;
	Math::Vector4				Color = { 1, 1, 1, 1 };	// テクスチャへ乗算
	float						Alpha = 1.0f;			// 動きのalpha。ディゾルブ有効時は線形フェードの代わりに消去の進行度(1=見える、0=消える)になる
	float						Time = 0.0f;			// 経過秒。ノイズのスクロールに使う
	float						DepthOffset = 0.0f;		// ビュー空間でカメラ方向へずらす量(+で手前)
	bool						DepthTest = true;

	ParticleNoiseParams			Noise;					// flags=0、またはNoiseTextureが無ければノイズ無効
	std::shared_ptr<KdTexture>	NoiseTexture;
};

//====================================================================
//
// 一枚テクスチャ(クアッド)エフェクト描画用シェーダー
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// KdShaderManagerが1個だけ保持し、全ShapeEffectInstanceで共有する。
// 頂点バッファは使わず、ワールド行列の定数バッファだけでクアッドを描く。
//
//====================================================================
class ShapeEffectShader
{
public:

	ShapeEffectShader() {}
	~ShapeEffectShader() { Release(); }

	ShapeEffectShader(const ShapeEffectShader&) = delete;
	ShapeEffectShader& operator=(const ShapeEffectShader&) = delete;

	bool Init();
	void Release();

	// info.Worldのローカル XY平面(1x1)にテクスチャを貼ったクアッドを1枚描く
	//	Alphaはエフェクト層(Defaultパス)内での使用を想定。それ以外のブレンドは加算
	void Draw(const ShapeEffectDrawInfo& info);

	bool IsInitialized() const { return m_initialized; }

private:

	bool	m_initialized = false;

	ID3D11VertexShader*	m_VS = nullptr;
	ID3D11PixelShader*	m_PS = nullptr;

	// HLSL側 cbShapeEffect(b5)と一致させる事
	struct cbShape
	{
		Math::Matrix	World;
		Math::Vector4	Color = { 1, 1, 1, 1 };
		float			DepthOffset = 0.0f;
		float			Alpha = 1.0f;
		float			Time = 0.0f;
		float			_pad = 0.0f;
	};
	KdConstantBuffer<cbShape> m_cb;

	// ノイズ設定(PS用)。HLSL側cbParticleNoise(inc_GPUParticleNoise.hlsli)と一致させる事
	struct cbNoise
	{
		UINT			NoiseFlags = 0;
		float			NoiseScale = 1.0f;
		Math::Vector2	NoiseScroll = { 0, 0 };

		float			MaskPower = 1.0f;
		float			DissolveStart = 0.5f;
		float			DissolveSoftness = 0.05f;
		float			EdgeWidth = 0.1f;

		Math::Vector4	EdgeColor = { 1, 0.5f, 0.1f, 1 };
	};
	KdConstantBuffer<cbNoise> m_cbNoise;

	static constexpr UINT kCBSlot = 5;
	static constexpr UINT kNoiseCBSlot = 4;	// GPUParticleShaderと同じ(HLSL側 b4)
};
