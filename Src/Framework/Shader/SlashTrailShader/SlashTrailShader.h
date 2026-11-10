#pragma once

#include "../GPUParticle/ParticleBuffer.h"	// ParticleBlendMode を流用する為

//====================================================================
//
// トレイル(斬撃の軌跡)描画用シェーダー(VS/PS/InputLayout)を、
// 全SlashTrailRendererで共有するクラス
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// KdGPUParticleShaderと同じく、KdShaderManagerが1個だけ保持して共有する
// (インスタンスごとに持つのは動的頂点バッファだけ)。
//
// ・VS：SlashTrail_VS.hlsl(CPU計算済みの頂点をView×Projで変換するだけ)
// ・PS(Alpha)：SlashTrail_PS_AlphaMasked.hlsl(カラーグレード除外マスクも出力)
// ・PS(Alpha以外)：SlashTrail_PS_AddMasked.hlsl(DoFブラー除外マスクも出力)
// ・どちらのPSも、パーティクルと同じノイズ(inc_GPUParticleNoise.hlsli)で消去(ディゾルブ)する
//
// 【使い方】
//   KdShaderManager::Init()内で m_slashTrailShader.Init() を1回呼ぶ。
//   SlashTrailRenderer::Draw()内で Begin(blendMode) → BindNoise() → Draw → UnbindNoise() → End()。
//====================================================================
class SlashTrailShader
{
public:

	SlashTrailShader() {}
	~SlashTrailShader() { Release(); }

	// コピー禁止(D3D11の生ポインタ資源を持つ為)
	SlashTrailShader(const SlashTrailShader&) = delete;
	SlashTrailShader& operator=(const SlashTrailShader&) = delete;

	bool Init();

	void Release();

	// VS/PS/InputLayoutのバインドのみ行う(頂点バッファのセットとドローは呼び出し側)
	//	PSはblendModeで切り替わる(Alpha：マスク版、それ以外：Add用マスク版)
	void Begin(ParticleBlendMode blendMode);

	// 現状は何もしない(Begin側で個別ステートを触っていない為。対称性のために用意)
	void End();

	// ノイズ(CB・テクスチャ・サンプラー)をPSへ設定する。直前のバインドは退避し、UnbindNoise()で戻す
	//	noise.flagsが0、またはnoiseTextureが無い場合は、ノイズ無効(flags=0)を書き込む
	//	(直前のパーティクル描画の設定を引きずらない為)
	void BindNoise(const ParticleNoiseParams& noise, const std::shared_ptr<KdTexture>& noiseTexture);

	void UnbindNoise();

	bool IsInitialized() const { return m_initialized; }

private:

	bool	m_initialized = false;

	ID3D11VertexShader*	m_VS = nullptr;
	ID3D11PixelShader*	m_PS_Masked = nullptr;		// Alpha用
	ID3D11PixelShader*	m_PS_AddMasked = nullptr;	// Alpha以外用
	ID3D11InputLayout*	m_inputLayout = nullptr;

	// ノイズ用(HLSL側 inc_GPUParticleNoise.hlsli の b4 / t2 / s1 と一致させる事)
	static constexpr UINT	kNoiseCBSlot = 4;
	static constexpr UINT	kNoiseTexSlot = 2;
	static constexpr UINT	kNoiseSamplerSlot = 1;

	// cbParticleNoiseと同じレイアウト(48バイト)
	struct NoiseCBData
	{
		UINT			flags;
		float			noiseScale;
		Math::Vector2	scroll;
		float			maskPower;
		float			dissolveStart;
		float			dissolveSoftness;
		float			edgeWidth;
		Math::Vector4	edgeColor;
	};
	static_assert(sizeof(NoiseCBData) == 48, "cbParticleNoiseのサイズと一致させる事");

	ID3D11Buffer*			m_noiseCB = nullptr;
	ID3D11SamplerState*		m_wrapSampler = nullptr;

	// BindNoise()で退避した、直前のバインド
	ID3D11Buffer*				m_prevCB = nullptr;
	ID3D11ShaderResourceView*	m_prevSRV = nullptr;
	ID3D11SamplerState*			m_prevSampler = nullptr;
};
