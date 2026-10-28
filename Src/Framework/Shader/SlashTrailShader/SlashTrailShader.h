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
// ・PS(Alpha)：GPUParticle_PS_Masked.hlslを流用(カラーグレード除外マスクも出力)
// ・PS(Alpha以外)：SlashTrail_PS_AddMasked.hlsl(DoFブラー除外マスクも出力)
//
// 【使い方】
//   KdShaderManager::Init()内で m_slashTrailShader.Init() を1回呼ぶ。
//   SlashTrailRenderer::Draw()内で Begin(blendMode) → Draw → End()。
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

	bool IsInitialized() const { return m_initialized; }

private:

	bool	m_initialized = false;

	ID3D11VertexShader*	m_VS = nullptr;
	ID3D11PixelShader*	m_PS_Masked = nullptr;		// Alpha用
	ID3D11PixelShader*	m_PS_AddMasked = nullptr;	// Alpha以外用
	ID3D11InputLayout*	m_inputLayout = nullptr;
};
