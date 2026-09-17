#include "Application/main.h"

#include "KdPreviewPostProcess.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// srcColorへカラーグレードを適用し、内部の出力テクスチャへ書き込む
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPreviewPostProcess::Apply(const std::shared_ptr<KdTexture>& srcColor)
{
	if (!srcColor) { return; }

	EnsureDummyMaskTex();

	ResizeResultTex(srcColor->GetWidth(), srcColor->GetHeight());

	D3D11_VIEWPORT vp = {};
	vp.Width = (float)m_width;
	vp.Height = (float)m_height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;

	// 本編用シングルトンのカラーグレードVS/PS/定数バッファ(パラメータも共通)をそのまま使い回す。
	// srcColor(プレビューの生の描画結果) → m_resultTex(グレーディング後)
	KdShaderManager::Instance().m_postProcessShader.ApplyColorGradeOnly(
		srcColor, m_dummyMaskTex, m_resultTex, &vp);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 出力テクスチャをwidth x heightで(必要なら)作り直す
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPreviewPostProcess::ResizeResultTex(int width, int height)
{
	if (width <= 0 || height <= 0) { return; }

	// サイズが変わっていなければ作り直さない
	if (width == m_width && height == m_height && m_resultTex) { return; }

	m_width = width;
	m_height = height;

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.Width = (UINT)width;
	desc.Height = (UINT)height;
	desc.CPUAccessFlags = 0;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	m_resultTex = std::make_shared<KdTexture>();
	m_resultTex->Create(desc);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// カラーグレードPSが要求するマスク入力(t1)用の1x1黒ダミーテクスチャを(未生成なら)作る
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPreviewPostProcess::EnsureDummyMaskTex()
{
	if (m_dummyMaskTex) { return; }

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.Format = DXGI_FORMAT_R8_UNORM;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.Width = 1;
	desc.Height = 1;
	desc.CPUAccessFlags = 0;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	m_dummyMaskTex = std::make_shared<KdTexture>();
	m_dummyMaskTex->Create(desc);

	// 黒(0)でクリアしておく = 「マスク無し・グレーディングを通常通り適用」を意味する
	// (本編のm_colorGradeMaskRTPackのクリア値と同じ規約)
	KdDirect3D::Instance().WorkDevContext()->ClearRenderTargetView(m_dummyMaskTex->WorkRTView(), kBlackColor);
}
