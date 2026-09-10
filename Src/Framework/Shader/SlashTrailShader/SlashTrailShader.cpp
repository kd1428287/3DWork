#include "Framework/KdFramework.h"

#include "SlashTrailShader.h"
#include "SlashTrailRenderer.h"	// SlashTrailRenderer::Vertex のレイアウト定義を流用する為(offsetの根拠)

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 初期化：VS/PS/InputLayoutの生成
//	※中身は旧SlashTrailRenderer::CreateShaders()からそのまま移設したもので、
//	  シェーダーバイナリ・レイアウト定義自体は一切変更していない
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool SlashTrailShader::Init()
{
	Release();

	ID3D11Device* Dev = KdDirect3D::Instance().WorkDev();

	// 頂点シェーダー(新規：SlashTrail_VS.hlsl)
	{
#include "SlashTrail_VS.shaderInc"

		if (FAILED(Dev->CreateVertexShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_VS)))
		{
			assert(0 && "SlashTrailShader：頂点シェーダー作成失敗");
			return false;
		}

		// 1頂点の詳細な情報(SlashTrailRenderer::Vertex構造体と一致させる事)
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

	// ピクセルシェーダー：KdGPUParticle_PS.hlslをそのまま流用
	{
#include "../GPUParticle/KdGPUParticle_PS.shaderInc"

		if (FAILED(Dev->CreatePixelShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS)))
		{
			assert(0 && "SlashTrailShader：ピクセルシェーダー作成失敗(KdGPUParticle_PS流用)");
			return false;
		}
	}

	m_initialized = true;

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 解放
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailShader::Release()
{
	KdSafeRelease(m_VS);
	KdSafeRelease(m_PS);
	KdSafeRelease(m_inputLayout);

	m_initialized = false;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// VS/PS/InputLayoutのバインド
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailShader::Begin()
{
	if (!m_initialized) { return; }

	KdShaderManager& shaderMgr = KdShaderManager::Instance();

	shaderMgr.SetVertexShader(m_VS);
	shaderMgr.SetPixelShader(m_PS);

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();
	DevCon->IASetInputLayout(m_inputLayout);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 対称性のための空実装(Begin()側でRS/DSS等の個別ステートを直接触っていない為)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailShader::End()
{
}
