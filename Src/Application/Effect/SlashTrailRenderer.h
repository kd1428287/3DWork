#pragma once

#include "GPUParticle.h"	// ParticleBlendMode を流用する為(同じ低レイヤー内で完結させる)
#include <vector>

//====================================================================
//
// トレイル(斬撃の軌跡)描画用のGPU資源を一元管理するクラス
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// GPUParticleと対になる、動的頂点バッファ版の描画クラス。
// ・パーティクルと違い、頂点(位置・UV・色)はCPU側(SlashTrailInstance)で
//   毎フレーム計算済みのものを渡されるだけなので、コンピュートシェーダーは使わない
// ・頂点シェーダーはSlashTrail_VS.hlsl(新規、ごく単純なView×Proj変換のみ)
// ・ピクセルシェーダーはGPUParticle_PS.hlslをそのまま流用する
//   (テクスチャ×頂点カラー、アルファにLifeRateを掛けるだけのUnlit PSで、
//    トレイルの要件と完全に一致する為、新規に書く必要が無かった)
//
// 【使い方】
//   SlashTrailRenderer renderer;
//   renderer.Init(maxVertexCount);   // SlashTrailParams::MaxSamples * 2 を渡す想定
//   :
//   // 毎フレーム、描画するなら
//   renderer.Draw(vertices, blendMode);
//
//====================================================================
class SlashTrailRenderer
{
public:

	// トレイル1頂点のデータ
	// ※HLSL側(SlashTrail_VS.hlsl の VSInput構造体)とレイアウトを必ず一致させる事
	struct Vertex
	{
		DirectX::SimpleMath::Vector3	Position;
		DirectX::SimpleMath::Vector2	UV;
		DirectX::SimpleMath::Vector4	Color;	// rgb + フェード込みのアルファ
	};

	SlashTrailRenderer() {}
	~SlashTrailRenderer() { Release(); }

	// コピー禁止(D3D11の生ポインタ資源を持つ為)
	SlashTrailRenderer(const SlashTrailRenderer&) = delete;
	SlashTrailRenderer& operator=(const SlashTrailRenderer&) = delete;

	// maxVertexCount：動的頂点バッファが受け入れられる最大頂点数
	//	(SlashTrailParams::MaxSamples * 2 を渡す想定。三角形ストリップ用に2頂点/サンプル)
	bool Init(UINT maxVertexCount);

	void Release();

	// verticesを頂点バッファへ書き込み、三角形ストリップとして描画する。
	//	verticesが4頂点(2サンプル)未満の場合は何もしない。
	//	blendModeでBlendStateを切り替える(GPUParticle::Drawと同じ要領)
	// ※事前にShaderManager::WriteCBCamera等でカメラ情報の転送が済んでいる事
	// ※テクスチャは未対応(TODO)。現状は白テクスチャを割り当て、頂点カラーのみで描画する
	void Draw(const std::vector<Vertex>& vertices, ParticleBlendMode blendMode);

private:

	bool CreateShaders();
	bool CreateVertexBuffer(UINT maxVertexCount);

	bool	m_initialized = false;
	UINT	m_maxVertexCount = 0;

	ID3D11VertexShader*	m_VS = nullptr;			// 新規(SlashTrail_VS.hlsl)
	ID3D11PixelShader*	m_PS = nullptr;			// GPUParticle_PS.hlslを流用
	ID3D11InputLayout*	m_inputLayout = nullptr;

	ID3D11Buffer*	m_vertexBuffer = nullptr;	// D3D11_USAGE_DYNAMIC。毎フレームMap/Unmapで書き換える
};
