#include "Framework/KdFramework.h"

#include "SlashTrailRenderer.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 初期化：頂点バッファ・シェーダーの生成
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool SlashTrailRenderer::Init(UINT maxVertexCount)
{
	Release();

	m_maxVertexCount = maxVertexCount;

	if (!CreateVertexBuffer(maxVertexCount)) { return false; }
	if (!CreateShaders()) { return false; }

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
// シェーダーの生成
//	・頂点シェーダーは新規(SlashTrail_VS.hlsl)：CPU側で計算済みの頂点をView×Projで
//	  変換するだけの単純なもの
//	・ピクセルシェーダーはKdGPUParticle_PS.hlslをそのまま流用する(同じ.shaderIncを
//	  インクルードするだけで済む。テクスチャ×頂点カラー、アルファにLifeRateを掛けるだけの
//	  UnlitなPSが、トレイルの要件と完全に一致する為、新規に書く必要が無かった)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool SlashTrailRenderer::CreateShaders()
{
	ID3D11Device* Dev = KdDirect3D::Instance().WorkDev();

	// 頂点シェーダー(新規)
	{
#include "KdSlashTrail_VS.shaderInc"

		if (FAILED(Dev->CreateVertexShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_VS)))
		{
			assert(0 && "SlashTrailRenderer：頂点シェーダー作成失敗");
			return false;
		}

		// 1頂点の詳細な情報(Vertex構造体と一致させる事)
		std::vector<D3D11_INPUT_ELEMENT_DESC> layout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		if (FAILED(Dev->CreateInputLayout(
			&layout[0], (UINT)layout.size(),
			compiledBuffer, sizeof(compiledBuffer),
			&m_inputLayout)))
		{
			assert(0 && "SlashTrailRenderer：CreateInputLayout失敗");
			return false;
		}
	}

	// ピクセルシェーダー：KdGPUParticle_PS.hlslをそのまま流用
	//	※実際の配置パスに合わせてinclude行を調整すること
	//	  (KdGPUParticleと同じフォルダに置く前提で相対パス無し表記にしている)
	{
#include "KdGPUParticle_PS.shaderInc"

		if (FAILED(Dev->CreatePixelShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS)))
		{
			assert(0 && "SlashTrailRenderer：ピクセルシェーダー作成失敗(KdGPUParticle_PS流用)");
			return false;
		}
	}

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 解放
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailRenderer::Release()
{
	SafeRelease(m_VS);
	SafeRelease(m_PS);
	SafeRelease(m_inputLayout);
	SafeRelease(m_vertexBuffer);

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
	ShaderManager& shaderMgr = ShaderManager::Instance();

	shaderMgr.SetVertexShader(m_VS);
	DevCon->IASetInputLayout(m_inputLayout);
	shaderMgr.SetPixelShader(m_PS);

	// TODO: SlashTrailParams::TexturePathからの解決が未実装の為、暫定で白テクスチャを割り当てる
	//	(ITextureProviderと同じ仕組みをSlashTrailInstance/Dispatcher側に用意すれば差し替えられる)
	ID3D11ShaderResourceView* whiteSRV = KdDirect3D::Instance().GetWhiteTex()->WorkSRView();
	DevCon->PSSetShaderResources(1, 1, &whiteSRV);

	// KdGPUParticle_PS.hlslが要求するサンプラースロット(s0)に合わせる
	shaderMgr.ChangeSamplerState(SamplerState::Linear_Clamp, 0);

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

	// SRVのバインド解除
	ID3D11ShaderResourceView* nullSRV = nullptr;
	DevCon->PSSetShaderResources(1, 1, &nullSRV);
}
