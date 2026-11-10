#include "Framework/KdFramework.h"

#include "SlashTrailShader.h"
#include "SlashTrailRenderer.h"	// SlashTrailRenderer::Vertex のレイアウト定義(offsetの根拠)

// VS/PS/InputLayoutの生成
bool SlashTrailShader::Init()
{
	Release();

	ID3D11Device* Dev = KdDirect3D::Instance().WorkDev();

	// 頂点シェーダー + InputLayout
	{
#include "SlashTrail_VS.shaderInc"

		if (FAILED(Dev->CreateVertexShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_VS)))
		{
			assert(0 && "SlashTrailShader：頂点シェーダー作成失敗");
			return false;
		}

		// SlashTrailRenderer::Vertex構造体と一致させる事
		std::vector<D3D11_INPUT_ELEMENT_DESC> layout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		if (FAILED(Dev->CreateInputLayout(
			&layout[0], (UINT)layout.size(),
			compiledBuffer, sizeof(compiledBuffer),
			&m_inputLayout)))
		{
			assert(0 && "SlashTrailShader：CreateInputLayout失敗");
			return false;
		}
	}

	// ピクセルシェーダー(Alpha用)：SlashTrail_PS_AlphaMasked.hlsl
	{
#include "SlashTrail_PS_AlphaMasked.shaderInc"

		if (FAILED(Dev->CreatePixelShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_Masked)))
		{
			assert(0 && "SlashTrailShader：ピクセルシェーダー作成失敗(SlashTrail_PS_AlphaMasked)");
			return false;
		}
	}

	// ピクセルシェーダー(Alpha以外用)：SlashTrail_PS_AddMasked.hlsl
	{
#include "SlashTrail_PS_AddMasked.shaderInc"

		if (FAILED(Dev->CreatePixelShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_AddMasked)))
		{
			assert(0 && "SlashTrailShader：ピクセルシェーダー作成失敗(SlashTrail_PS_AddMasked)");
			return false;
		}
	}

	// ノイズ用の定数バッファ(cbParticleNoiseと同じ48バイト)とラップサンプラー
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(NoiseCBData);
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		if (FAILED(Dev->CreateBuffer(&desc, nullptr, &m_noiseCB)))
		{
			assert(0 && "SlashTrailShader：ノイズ用定数バッファ作成失敗");
			return false;
		}

		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		if (FAILED(Dev->CreateSamplerState(&samplerDesc, &m_wrapSampler)))
		{
			assert(0 && "SlashTrailShader：ノイズ用サンプラー作成失敗");
			return false;
		}
	}

	m_initialized = true;

	return true;
}

void SlashTrailShader::Release()
{
	KdSafeRelease(m_VS);
	KdSafeRelease(m_PS_Masked);
	KdSafeRelease(m_PS_AddMasked);
	KdSafeRelease(m_inputLayout);
	KdSafeRelease(m_noiseCB);
	KdSafeRelease(m_wrapSampler);

	m_initialized = false;
}

// VS/PS/InputLayoutのバインド
void SlashTrailShader::Begin(ParticleBlendMode blendMode)
{
	if (!m_initialized) { return; }

	KdShaderManager& shaderMgr = KdShaderManager::Instance();

	shaderMgr.SetVertexShader(m_VS);
	shaderMgr.SetPixelShader(blendMode == ParticleBlendMode::Alpha ? m_PS_Masked : m_PS_AddMasked);

	KdDirect3D::Instance().WorkDevContext()->IASetInputLayout(m_inputLayout);
}

void SlashTrailShader::End()
{
}

// ノイズの設定：直前のバインドを退避してからCB・テクスチャ・サンプラーを設定する
void SlashTrailShader::BindNoise(const ParticleNoiseParams& noise, const std::shared_ptr<KdTexture>& noiseTexture)
{
	if (!m_initialized) { return; }

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

	const bool enabled = (noise.flags != 0) && noiseTexture;

	NoiseCBData cb = {};
	cb.flags = enabled ? noise.flags : 0;
	cb.noiseScale = noise.noiseScale;
	cb.scroll = noise.scroll;
	cb.maskPower = noise.maskPower;
	cb.dissolveStart = noise.dissolveStart;
	cb.dissolveSoftness = noise.dissolveSoftness;
	cb.edgeWidth = noise.edgeWidth;
	cb.edgeColor = noise.edgeColor;

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (SUCCEEDED(DevCon->Map(m_noiseCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &cb, sizeof(cb));
		DevCon->Unmap(m_noiseCB, 0);
	}

	DevCon->PSGetConstantBuffers(kNoiseCBSlot, 1, &m_prevCB);
	DevCon->PSGetShaderResources(kNoiseTexSlot, 1, &m_prevSRV);
	DevCon->PSGetSamplers(kNoiseSamplerSlot, 1, &m_prevSampler);

	ID3D11ShaderResourceView* srv = enabled ? noiseTexture->WorkSRView() : nullptr;

	DevCon->PSSetConstantBuffers(kNoiseCBSlot, 1, &m_noiseCB);
	DevCon->PSSetShaderResources(kNoiseTexSlot, 1, &srv);
	DevCon->PSSetSamplers(kNoiseSamplerSlot, 1, &m_wrapSampler);
}

// BindNoise()で退避したバインドを元に戻す
void SlashTrailShader::UnbindNoise()
{
	if (!m_initialized) { return; }

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

	DevCon->PSSetConstantBuffers(kNoiseCBSlot, 1, &m_prevCB);
	DevCon->PSSetShaderResources(kNoiseTexSlot, 1, &m_prevSRV);
	DevCon->PSSetSamplers(kNoiseSamplerSlot, 1, &m_prevSampler);

	KdSafeRelease(m_prevCB);
	KdSafeRelease(m_prevSRV);
	KdSafeRelease(m_prevSampler);
}
