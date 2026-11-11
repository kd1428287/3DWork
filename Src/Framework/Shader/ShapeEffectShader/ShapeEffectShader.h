#pragma once

#include "../GPUParticle/ParticleBuffer.h"	// ParticleBlendMode を流用する為

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

	// worldのローカルXY平面(1x1)にテクスチャを貼ったクアッドを1枚描く。colorはテクスチャへ乗算される
	//	Alphaはエフェクト層(Defaultパス)内での使用を想定。それ以外のブレンドは加算
	void Draw(const std::shared_ptr<KdTexture>& texture, ParticleBlendMode blendMode,
		const Math::Matrix& world, const Math::Vector4& color);

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
	};
	KdConstantBuffer<cbShape> m_cb;

	static constexpr UINT kCBSlot = 5;
};
