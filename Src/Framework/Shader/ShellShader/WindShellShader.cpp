#include "Framework/KdFramework.h"

#include "WindShellShader.h"

// VS/PS/InputLayout・グリッドメッシュ・定数バッファの生成
bool WindShellShader::Init()
{
	Release();

	ID3D11Device* Dev = KdDirect3D::Instance().WorkDev();

	// 頂点シェーダー + InputLayout
	{
#include "WindShell_VS.shaderInc"

		if (FAILED(Dev->CreateVertexShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_VS)))
		{
			assert(0 && "WindShellShader：頂点シェーダー作成失敗");
			Release();
			return false;
		}

		// 頂点はUV(x=角度 y=軸方向)だけ。形は全てVSで作る
		std::vector<D3D11_INPUT_ELEMENT_DESC> layout =
		{
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		if (FAILED(Dev->CreateInputLayout(
			&layout[0], (UINT)layout.size(),
			compiledBuffer, sizeof(compiledBuffer),
			&m_inputLayout)))
		{
			assert(0 && "WindShellShader：CreateInputLayout失敗");
			Release();
			return false;
		}
	}

	// ピクセルシェーダー
	{
#include "WindShell_PS.shaderInc"

		if (FAILED(Dev->CreatePixelShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS)))
		{
			assert(0 && "WindShellShader：ピクセルシェーダー作成失敗");
			Release();
			return false;
		}
	}

	if (!CreateGridMesh())
	{
		Release();
		return false;
	}

	m_cb.Create();

	m_initialized = true;

	return true;
}

// グリッドメッシュの生成(継ぎ目は頂点を重複させてUVを連続させる)
bool WindShellShader::CreateGridMesh()
{
	struct Vertex { Math::Vector2 UV; };

	std::vector<Vertex> vertices;
	std::vector<UINT> indices;
	vertices.reserve((kSegU + 1) * (kSegV + 1));
	indices.reserve(kSegU * kSegV * 6);

	for (int j = 0; j <= kSegV; ++j)
	{
		for (int i = 0; i <= kSegU; ++i)
		{
			vertices.push_back({ { (float)i / kSegU, (float)j / kSegV } });
		}
	}

	for (int j = 0; j < kSegV; ++j)
	{
		for (int i = 0; i < kSegU; ++i)
		{
			const UINT a = j * (kSegU + 1) + i;
			const UINT b = a + 1;
			const UINT c = a + (kSegU + 1);
			const UINT d = c + 1;
			indices.insert(indices.end(), { a, c, b, b, c, d });
		}
	}

	m_indexCount = (UINT)indices.size();

	ID3D11Device* Dev = KdDirect3D::Instance().WorkDev();

	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_IMMUTABLE;

	D3D11_SUBRESOURCE_DATA init = {};

	desc.ByteWidth = (UINT)(sizeof(Vertex) * vertices.size());
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	init.pSysMem = vertices.data();
	if (FAILED(Dev->CreateBuffer(&desc, &init, &m_vertexBuffer)))
	{
		assert(0 && "WindShellShader：頂点バッファ作成失敗");
		return false;
	}

	desc.ByteWidth = (UINT)(sizeof(UINT) * indices.size());
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	init.pSysMem = indices.data();
	if (FAILED(Dev->CreateBuffer(&desc, &init, &m_indexBuffer)))
	{
		assert(0 && "WindShellShader：インデックスバッファ作成失敗");
		return false;
	}

	return true;
}

void WindShellShader::Release()
{
	KdSafeRelease(m_VS);
	KdSafeRelease(m_PS);
	KdSafeRelease(m_inputLayout);
	KdSafeRelease(m_vertexBuffer);
	KdSafeRelease(m_indexBuffer);

	m_cb.Release();

	m_indexCount = 0;
	m_initialized = false;
}

// 1レイヤーを描画：ステート設定→描画→復元。呼び出し順はSlashTrailRenderer::Drawと同じ
void WindShellShader::Draw(const WindShellParams& params, ParticleBlendMode blendMode,
	const std::shared_ptr<KdTexture>& streakTexture,
	const ParticleNoiseParams& noise, const std::shared_ptr<KdTexture>& noiseTexture,
	const PerlinNoiseGPU& perlin, bool effectLayerBlend)
{
	if (!m_initialized || !streakTexture) { return; }

	KdShaderManager& shaderMgr = KdShaderManager::Instance();

	// ノイズの退避・復元はトレイルのシェーダーを借りる
	if (!shaderMgr.m_slashTrailShader.IsInitialized()) { return; }

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

	// 定数バッファ
	m_cb.Work() = params;
	m_cb.Write();
	shaderMgr.SetVSConstantBuffer(kCBSlot, m_cb.GetAddress());
	shaderMgr.SetPSConstantBuffer(kCBSlot, m_cb.GetAddress());

	// パイプライン
	shaderMgr.SetVertexShader(m_VS);
	shaderMgr.SetPixelShader(m_PS);
	DevCon->IASetInputLayout(m_inputLayout);

	// 消去ノイズ(PSのb4/t2/s1)
	shaderMgr.m_slashTrailShader.BindNoise(noise, noiseTexture);

	// 順列テーブル(VSのt10)。直前のバインドを退避する
	ID3D11ShaderResourceView* prevPerm = nullptr;
	DevCon->VSGetShaderResources(KdShaderManager::PerlinPermSlot, 1, &prevPerm);
	perlin.BindVS(DevCon, KdShaderManager::PerlinPermSlot);

	// 筋テクスチャ(PSのt1)。サンプラーはBindNoiseが設定するs1(wrap)を使う
	ID3D11ShaderResourceView* srv = streakTexture->WorkSRView();
	DevCon->PSSetShaderResources(kStreakTexSlot, 1, &srv);

	shaderMgr.ChangeRasterizerState(KdRasterizerState::CullNone);

	// ※通常はマスク(SV_Target1)への書き込みを許可するAlphaMaskedを使う事(トレイルと同じ)
	KdBlendState blendState = effectLayerBlend ? KdBlendState::Alpha : KdBlendState::AlphaMasked;
	if (blendMode == ParticleBlendMode::Add) { blendState = effectLayerBlend ? KdBlendState::EffectAdd : KdBlendState::Add; }
	else if (blendMode == ParticleBlendMode::Multiply) { blendState = KdBlendState::Multiply; }

	shaderMgr.ChangeBlendState(blendState);
	shaderMgr.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);

	UINT stride = sizeof(Math::Vector2);
	UINT offset = 0;
	DevCon->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
	DevCon->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	DevCon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DevCon->DrawIndexed(m_indexCount, 0, 0);

	DevCon->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);

	shaderMgr.UndoDepthStencilState();
	shaderMgr.UndoBlendState();
	shaderMgr.UndoRasterizerState();

	// SRVの復元・解除
	DevCon->VSSetShaderResources(KdShaderManager::PerlinPermSlot, 1, &prevPerm);
	KdSafeRelease(prevPerm);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	DevCon->PSSetShaderResources(kStreakTexSlot, 1, &nullSRV);

	shaderMgr.m_slashTrailShader.UnbindNoise();
}
