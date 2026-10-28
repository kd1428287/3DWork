#include "Framework/KdFramework.h"

#include "SlashTrailRenderer.h"

// 初期化：頂点バッファの生成
bool SlashTrailRenderer::Init(UINT maxVertexCountPerLayer)
{
	Release();

	m_maxVertexCount = maxVertexCountPerLayer;

	if (!CreateVertexBuffer(maxVertexCountPerLayer)) { return false; }

	m_initialized = true;

	return true;
}

// 動的頂点バッファの生成(D3D11_USAGE_DYNAMIC。墨・芯の2レイヤーぶんを確保する)
bool SlashTrailRenderer::CreateVertexBuffer(UINT maxVertexCountPerLayer)
{
	ID3D11Device* Dev = KdDirect3D::Instance().WorkDev();

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = sizeof(Vertex) * maxVertexCountPerLayer * 2;
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

void SlashTrailRenderer::Release()
{
	KdSafeRelease(m_vertexBuffer);

	m_initialized = false;
	m_maxVertexCount = 0;
	m_inkCount = 0;
	m_coreCount = 0;
}

// 頂点の書き込み：墨を先頭、芯をm_maxVertexCount個目から置く
void SlashTrailRenderer::Upload(const std::vector<Vertex>& inkVertices, const std::vector<Vertex>& coreVertices)
{
	m_inkCount = 0;
	m_coreCount = 0;

	if (!m_initialized) { return; }

	const UINT inkCount = (UINT)std::min<size_t>(inkVertices.size(), m_maxVertexCount);
	const UINT coreCount = (UINT)std::min<size_t>(coreVertices.size(), m_maxVertexCount);
	if (inkCount == 0 && coreCount == 0) { return; }

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(DevCon->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) { return; }

	Vertex* dst = static_cast<Vertex*>(mapped.pData);
	if (inkCount > 0) { memcpy(dst, inkVertices.data(), sizeof(Vertex) * inkCount); }
	if (coreCount > 0) { memcpy(dst + m_maxVertexCount, coreVertices.data(), sizeof(Vertex) * coreCount); }

	DevCon->Unmap(m_vertexBuffer, 0);

	m_inkCount = inkCount;
	m_coreCount = coreCount;
}

// 描画：Upload済みの頂点を三角形ストリップとして描画する
void SlashTrailRenderer::Draw(Layer layer, ParticleBlendMode blendMode, const std::shared_ptr<KdTexture>& texture)
{
	if (!m_initialized) { return; }

	const UINT vertexCount = (layer == Layer::Ink) ? m_inkCount : m_coreCount;
	const UINT startVertex = (layer == Layer::Ink) ? 0 : m_maxVertexCount;

	// 三角形ストリップを組むには最低2点(4頂点)必要
	if (vertexCount < 4) { return; }

	KdShaderManager& shaderMgr = KdShaderManager::Instance();
	if (!shaderMgr.m_slashTrailShader.IsInitialized()) { return; }

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

	shaderMgr.m_slashTrailShader.Begin(blendMode);

	// テクスチャ未指定なら白テクスチャ(頂点カラーのみで描画)
	ID3D11ShaderResourceView* srv = texture ? texture->WorkSRView() : KdDirect3D::Instance().GetWhiteTex()->WorkSRView();
	DevCon->PSSetShaderResources(1, 1, &srv);

	// PSが要求するサンプラースロット(s0)に合わせる
	shaderMgr.ChangeSamplerState(KdSamplerState::Linear_Clamp, 0);
	shaderMgr.ChangeRasterizerState(KdRasterizerState::CullNone);

	// ※Alphaはマスク(SV_Target1)への書き込みを許可するAlphaMaskedを使う事
	KdBlendState blendState = KdBlendState::AlphaMasked;
	if (blendMode == ParticleBlendMode::Add) { blendState = KdBlendState::Add; }
	else if (blendMode == ParticleBlendMode::Multiply) { blendState = KdBlendState::Multiply; }

	shaderMgr.ChangeBlendState(blendState);
	shaderMgr.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	DevCon->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
	DevCon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	DevCon->Draw(vertexCount, startVertex);

	shaderMgr.UndoDepthStencilState();
	shaderMgr.UndoBlendState();
	shaderMgr.UndoSamplerState();
	shaderMgr.UndoRasterizerState();

	// SRVのバインド解除
	ID3D11ShaderResourceView* nullSRV = nullptr;
	DevCon->PSSetShaderResources(1, 1, &nullSRV);

	shaderMgr.m_slashTrailShader.End();
}
