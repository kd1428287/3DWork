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

	// ピクセルシェーダー(Alpha用)：GPUParticle_PS_Masked.hlslを流用
	{
#include "../GPUParticle/GPUParticle_PS_Masked.shaderInc"

		if (FAILED(Dev->CreatePixelShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_Masked)))
		{
			assert(0 && "SlashTrailShader：ピクセルシェーダー作成失敗(GPUParticle_PS_Masked流用)");
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

	m_initialized = true;

	return true;
}

void SlashTrailShader::Release()
{
	KdSafeRelease(m_VS);
	KdSafeRelease(m_PS_Masked);
	KdSafeRelease(m_PS_AddMasked);
	KdSafeRelease(m_inputLayout);

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
