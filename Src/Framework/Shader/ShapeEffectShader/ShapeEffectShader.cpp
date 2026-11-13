#include "Framework/KdFramework.h"

#include "ShapeEffectShader.h"
#include "Framework/Shader/KdShaderManager.h"

// VS/PS・定数バッファの生成
bool ShapeEffectShader::Init()
{
	Release();

	ID3D11Device* Dev = KdDirect3D::Instance().WorkDev();

	// 頂点シェーダー(頂点バッファ未使用のためInputLayoutは不要)
	{
#include "ShapeEffect_VS.shaderInc"

		if (FAILED(Dev->CreateVertexShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_VS)))
		{
			assert(0 && "ShapeEffectShader：頂点シェーダー作成失敗");
			return false;
		}
	}

	// ピクセルシェーダー
	{
#include "ShapeEffect_PS.shaderInc"

		if (FAILED(Dev->CreatePixelShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS)))
		{
			assert(0 && "ShapeEffectShader：ピクセルシェーダー作成失敗");
			return false;
		}
	}

	m_cb.Create();
	m_cbNoise.Create();

	m_initialized = true;

	return true;
}

void ShapeEffectShader::Release()
{
	KdSafeRelease(m_VS);
	KdSafeRelease(m_PS);

	m_cb.Release();
	m_cbNoise.Release();

	m_initialized = false;
}

// クアッド1枚を描画
void ShapeEffectShader::Draw(const ShapeEffectDrawInfo& info)
{
	if (!m_initialized || !info.Texture) { return; }

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

	KdShaderManager& shaderMgr = KdShaderManager::Instance();

	shaderMgr.SetVertexShader(m_VS);
	shaderMgr.SetPixelShader(m_PS);
	DevCon->IASetInputLayout(nullptr);

	cbShape& cb = m_cb.Work();
	cb.World = info.World;
	cb.Color = info.Color;
	cb.DepthOffset = info.DepthOffset;
	cb.Alpha = info.Alpha;
	cb.Time = info.Time;
	m_cb.Write();
	DevCon->VSSetConstantBuffers(kCBSlot, 1, m_cb.GetAddress());
	DevCon->PSSetConstantBuffers(kCBSlot, 1, m_cb.GetAddress());

	DevCon->PSSetShaderResources(0, 1, info.Texture->WorkSRViewAddress());
	shaderMgr.ChangeSamplerState(KdSamplerState::Linear_Clamp, 0);

	// ノイズ設定(無効時もCBを毎回書き、前回の値を残さない)
	const bool useNoise = info.Noise.flags != 0 && info.NoiseTexture;
	cbNoise& nz = m_cbNoise.Work();
	nz.NoiseFlags = useNoise ? info.Noise.flags : 0;
	nz.NoiseScale = info.Noise.noiseScale;
	nz.NoiseScroll = info.Noise.scroll;
	nz.MaskPower = info.Noise.maskPower;
	nz.DissolveStart = info.Noise.dissolveStart;
	nz.DissolveSoftness = info.Noise.dissolveSoftness;
	nz.EdgeWidth = info.Noise.edgeWidth;
	nz.EdgeColor = info.Noise.edgeColor;
	m_cbNoise.Write();
	DevCon->PSSetConstantBuffers(kNoiseCBSlot, 1, m_cbNoise.GetAddress());

	if (useNoise) { DevCon->PSSetShaderResources(2, 1, info.NoiseTexture->WorkSRViewAddress()); }

	// ノイズはタイリングするのでWrap系サンプラーをスロット1へ
	shaderMgr.ChangeSamplerState(KdSamplerState::Anisotropic_Wrap, 1);

	// エフェクト層へ描く間は、加算も被覆率(A)を変えない専用ブレンドを使う(パーティクルと同じ)
	const bool effectLayer = shaderMgr.m_postProcessShader.IsEffectLayerActive();
	const bool isAlpha = (info.BlendMode == ParticleBlendMode::Alpha);
	KdBlendState blendState = isAlpha ? KdBlendState::Alpha : (effectLayer ? KdBlendState::EffectAdd : KdBlendState::Add);
	shaderMgr.ChangeBlendState(blendState);
	shaderMgr.ChangeDepthStencilState(info.DepthTest ? KdDepthStencilState::ZWriteDisable : KdDepthStencilState::ZDisable);
	shaderMgr.ChangeRasterizerState(KdRasterizerState::CullNone);

	DevCon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	DevCon->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	DevCon->Draw(4, 0);

	shaderMgr.UndoRasterizerState();
	shaderMgr.UndoDepthStencilState();
	shaderMgr.UndoBlendState();
	shaderMgr.UndoSamplerState(1);	// ノイズ用(スロット1)
	shaderMgr.UndoSamplerState(0);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	DevCon->PSSetShaderResources(0, 1, &nullSRV);
	DevCon->PSSetShaderResources(2, 1, &nullSRV);
}
