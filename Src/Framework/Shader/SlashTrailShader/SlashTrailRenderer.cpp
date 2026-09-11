#include "Framework/KdFramework.h"

#include "SlashTrailRenderer.h"
#include "SlashTrailShader.h"	// 共有VS/PS/InputLayout。KdShaderManagerが1個だけ保持する

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 初期化：動的頂点バッファの生成のみ行う
//	※シェーダー(VS/PS/InputLayout)はSlashTrailShader側へ移設した為、
//	  ここでは生成しない(KdShaderManager初期化時に別途1回だけ済んでいる想定)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool SlashTrailRenderer::Init(UINT maxVertexCount)
{
	Release();

	m_maxVertexCount = maxVertexCount;

	if (!CreateVertexBuffer(maxVertexCount)) { return false; }

	m_initialized = true;

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 動的頂点バッファの生成(D3D11_USAGE_DYNAMIC。毎フレームMap/Unmapで書き換える)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool SlashTrailRenderer::CreateVertexBuffer(UINT maxVertexCount)
{
	ID3D11Device* Dev = KdDirect3D::Instance().WorkDev();

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = sizeof(Vertex) * maxVertexCount;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(Dev->CreateBuffer(&desc, nullptr, &m_vertexBuffer)))
	{
		assert(0 && "SlashTrailRenderer：頂点バッファ作成失敗");
		return false;
	}

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 解放
//	※シェーダー資源はここでは持っていない(SlashTrailShader側で管理)ので触らない
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailRenderer::Release()
{
	KdSafeRelease(m_vertexBuffer);

	m_initialized = false;
	m_maxVertexCount = 0;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画：verticesを動的頂点バッファへ書き込み、三角形ストリップとして描画する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailRenderer::Draw(const std::vector<Vertex>& vertices, KdParticleBlendMode blendMode)
{
	if (!m_initialized) { return; }

	// 三角形ストリップを組むには最低2サンプル(4頂点)必要
	if (vertices.size() < 4) { return; }

	KdShaderManager& shaderMgr = KdShaderManager::Instance();
	SlashTrailShader& trailShader = shaderMgr.m_slashTrailShader;

	// 共有シェーダーが未初期化(KdShaderManager側の初期化漏れ)なら描画しない安全弁
	if (!trailShader.IsInitialized()) { return; }

	// 確保済みバッファ容量を超える分は安全のため切り捨てる
	// (呼び出し元でMaxSamplesを守っていれば通常発生しない想定の安全弁)
	const UINT vertexCount = (vertices.size() < (size_t)m_maxVertexCount)
		? (UINT)vertices.size() : m_maxVertexCount;

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

	//------------------------------------------
	// 動的頂点バッファへ書き込み
	//------------------------------------------
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(DevCon->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		return;
	}
	memcpy(mapped.pData, vertices.data(), sizeof(Vertex) * vertexCount);
	DevCon->Unmap(m_vertexBuffer, 0);

	//------------------------------------------
	// 描画
	//------------------------------------------
	trailShader.Begin();	// VS/PS/InputLayoutのバインド(全インスタンス共有)

	// TODO: SlashTrailParams::TexturePathからの解決が未実装の為、暫定で白テクスチャを割り当てる
	//	(ITextureProviderと同じ仕組みをSlashTrailInstance/Dispatcher側に用意すれば差し替えられる)
	ID3D11ShaderResourceView* whiteSRV = KdDirect3D::Instance().GetWhiteTex()->WorkSRView();
	DevCon->PSSetShaderResources(1, 1, &whiteSRV);

	std::shared_ptr<KdTexture> texture = 
		KdAssets::Instance().m_textures.GetData("Asset/Textures/Game/Effect/Trail4.png");
	if (texture)
	{
		DevCon->PSSetShaderResources(1, 1, texture->WorkSRViewAddress());
	}

	// KdGPUParticle_PS.hlslが要求するサンプラースロット(s0)に合わせる
	shaderMgr.ChangeSamplerState(KdSamplerState::Linear_Clamp, 0);
	shaderMgr.ChangeRasterizerState(KdRasterizerState::CullNone);

	// ブレンドモードの切り替え・Z書き込み無効(KdGPUParticle::Drawと同じ要領)
	const KdBlendState blendState = (blendMode == KdParticleBlendMode::Alpha) ? KdBlendState::Alpha : KdBlendState::Add;
	shaderMgr.ChangeBlendState(blendState);
	shaderMgr.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	DevCon->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
	DevCon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	DevCon->Draw(vertexCount, 0);

	shaderMgr.UndoDepthStencilState();
	shaderMgr.UndoBlendState();
	shaderMgr.UndoSamplerState();
	shaderMgr.UndoRasterizerState();

	// SRVのバインド解除
	ID3D11ShaderResourceView* nullSRV = nullptr;
	DevCon->PSSetShaderResources(1, 1, &nullSRV);

	trailShader.End();
}