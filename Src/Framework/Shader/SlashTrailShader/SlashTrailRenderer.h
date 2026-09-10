#pragma once

#include "../GPUParticle/KdGPUParticle.h"	// KdParticleBlendMode を流用する為(同じ低レイヤー内で完結させる)
#include <vector>

//====================================================================
//
// トレイル(斬撃の軌跡)描画用のGPU資源を一元管理するクラス
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// KdGPUParticleと対になる、動的頂点バッファ版の描画クラス。
// ・パーティクルと違い、頂点(位置・UV・色)はCPU側(SlashTrailInstance)で
//   毎フレーム計算済みのものを渡されるだけなので、コンピュートシェーダーは使わない
// ・頂点シェーダー(SlashTrail_VS.hlsl)・ピクセルシェーダー(KdGPUParticle_PS.hlsl流用)は
//   KdShaderManagerが保持するKdSlashTrailShaderで一元管理する(全インスタンス共有)。
//   ※以前は本クラスがインスタンスごとにVS/PS/InputLayoutを生成していたが、
//     攻撃のたびにシェーダーオブジェクトを作り直す無駄があった為、
//     KdStandardShader等と同じ「シェーダー共有・頂点バッファのみインスタンス固有」の
//     形に統一した(KdSlashTrailShaderへ分離)。本クラスが持つのは
//     動的頂点バッファのみになった
//
// 【使い方】
//   // 事前に1回、KdShaderManager::Instance().GetSlashTrailShader().Init() 済みであること
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
	//	 (レイアウト定義自体はKdSlashTrailShader::Init()側で保持している)
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
	//	blendModeでKdBlendStateを切り替える(KdGPUParticle::Drawと同じ要領)
	// ※事前にShaderManager::WriteCBCamera等でカメラ情報の転送が済んでいる事
	// ※KdShaderManager::Instance().GetSlashTrailShader()が初期化済みである事
	// ※テクスチャは未対応(TODO)。現状は白テクスチャを割り当て、頂点カラーのみで描画する
	void Draw(const std::vector<Vertex>& vertices, KdParticleBlendMode blendMode);

private:

	bool CreateVertexBuffer(UINT maxVertexCount);

	bool	m_initialized = false;
	UINT	m_maxVertexCount = 0;

	ID3D11Buffer* m_vertexBuffer = nullptr;	// D3D11_USAGE_DYNAMIC。毎フレームMap/Unmapで書き換える
};