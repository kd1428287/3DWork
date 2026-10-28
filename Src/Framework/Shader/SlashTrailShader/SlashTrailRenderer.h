#pragma once

#include "../GPUParticle/ParticleBuffer.h"	// ParticleBlendMode を流用する為
#include <vector>

//====================================================================
//
// トレイル(斬撃の軌跡)の動的頂点バッファを保持して描画するクラス
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// ・VS/PS/InputLayoutはSlashTrailShader(KdShaderManagerが共有)が持つ。ここは頂点バッファだけ
// ・頂点バッファには墨レイヤーと芯レイヤーの2層ぶんを確保し、1フレームに1回Upload()で書き込む。
//   Draw()は何度呼んでもバッファへの書き込みはしない(Lit/Bright両パスから呼べる)
//
// 【使い方】
//   renderer.Init(maxVertexCountPerLayer);
//   renderer.Upload(inkVertices, coreVertices);                 // 頂点を作り直した時(毎フレーム1回)
//   renderer.Draw(SlashTrailRenderer::Layer::Ink, blendMode, texture);   // 描画パスごと
//====================================================================
class SlashTrailRenderer
{
public:

	// トレイル1頂点のデータ
	// ※HLSL側(SlashTrail_VS.hlsl の VSInput構造体)とレイアウトを必ず一致させる事
	struct Vertex
	{
		Math::Vector3   Position;
		Math::Vector2	UV;
		Math::Vector4	Color;	// rgb + フェード込みのアルファ
	};

	enum class Layer { Ink, Core };

	SlashTrailRenderer() {}
	~SlashTrailRenderer() { Release(); }

	// コピー禁止(D3D11の生ポインタ資源を持つ為)
	SlashTrailRenderer(const SlashTrailRenderer&) = delete;
	SlashTrailRenderer& operator=(const SlashTrailRenderer&) = delete;

	// maxVertexCountPerLayer：1レイヤーが受け入れられる最大頂点数(三角形ストリップ用に2頂点/点)
	bool Init(UINT maxVertexCountPerLayer);

	void Release();

	// 2レイヤーぶんの頂点を頂点バッファへ書き込む。容量を超える分は切り捨てる
	void Upload(const std::vector<Vertex>& inkVertices, const std::vector<Vertex>& coreVertices);

	// Upload済みの頂点を三角形ストリップとして描画する。4頂点未満のレイヤーは何もしない。
	//	textureがnullptrなら白テクスチャで描画する(頂点カラーのみ)
	// ※事前にShaderManager::WriteCBCamera等でカメラ情報の転送が済んでいる事
	void Draw(Layer layer, ParticleBlendMode blendMode, const std::shared_ptr<KdTexture>& texture);

private:

	bool CreateVertexBuffer(UINT maxVertexCountPerLayer);

	bool	m_initialized = false;
	UINT	m_maxVertexCount = 0;	// 1レイヤーぶんの容量

	UINT	m_inkCount = 0;
	UINT	m_coreCount = 0;

	ID3D11Buffer* m_vertexBuffer = nullptr;	// D3D11_USAGE_DYNAMIC。[墨レイヤー][芯レイヤー]の順
};
