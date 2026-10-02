#include "Application/main.h"

#include "KdPreviewPostProcess.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// srcColorへ(渡されていればBloom→)カラーグレードを適用し、内部の出力テクスチャへ書き込む
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPreviewPostProcess::Apply(const std::shared_ptr<KdTexture>& srcColor, const std::shared_ptr<KdTexture>& srcBright)
{
	if (!srcColor) { return; }

	EnsureDummyMaskTex();

	ResizeResultTex(srcColor->GetWidth(), srcColor->GetHeight());

	// カラーグレードより前に、Bloomをsrc Colorへ直接加算合成しておく
	// (ApplyColorGradeOnly()はsrcColorを読んでm_resultTexへ書くだけなので、
	//  先にsrcColor自体へBloomを焼き込んでおけば、以降は従来通りのカラーグレード処理で済む)
	if (srcBright)
	{
		ApplyBloom(srcColor, srcBright);
	}

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
// srcBrightをぼかしながら段階的に合成し、srcColorへ加算合成する
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 本編のKdPostProcessShader::LightBloomProcess()と同じ考え方(ぼかしを繰り返しかけた
// 複数段階を加算合成してにじませる)だが、シーン全体からの輝度抽出(BrightPS)は行わず、
// 呼び出し側が既に「Bright専用」として加算描画済みのsrcBrightをそのまま使う点が異なる。
// これは、エフェクトプレビューが元々パーティクルしか描いていない(通常描画物の輝度を
// 別途抽出する必要が無い)ことを前提にした簡略化
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPreviewPostProcess::ApplyBloom(const std::shared_ptr<KdTexture>& srcColor, const std::shared_ptr<KdTexture>& srcBright)
{
	ResizeBloomTex(srcColor->GetWidth(), srcColor->GetHeight());

	ID3D11DeviceContext* context = KdDirect3D::Instance().WorkDevContext();

	// 退避
	ID3D11RenderTargetView* savedRTV = nullptr;
	ID3D11DepthStencilView* savedDSV = nullptr;
	context->OMGetRenderTargets(1, &savedRTV, &savedDSV);

	UINT savedVPNum = 1;
	D3D11_VIEWPORT savedVP = {};
	context->RSGetViewports(&savedVPNum, &savedVP);

	D3D11_VIEWPORT vp = {};
	vp.Width = (float)m_bloomWidth;
	vp.Height = (float)m_bloomHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;

	// ぼかしのピラミッドを作る(段数・半径は軽量化のため本編より少ない値にしてある)
	std::shared_ptr<KdTexture> srcTex = srcBright;
	for (int i = 0; i < kBloomNum; ++i)
	{
		KdShaderManager::Instance().m_postProcessShader.GenerateBlurTexture(
			srcTex, m_bloomTex[i], vp, kBloomBlurRadius);

		srcTex = m_bloomTex[i];
	}

	// 色本体(srcColor)へ加算合成する
	ID3D11RenderTargetView* rtvs[] = { srcColor->WorkRTView() };
	context->OMSetRenderTargets(1, rtvs, nullptr);
	context->RSSetViewports(1, &vp);

	KdShaderManager::Instance().ChangeSamplerState(KdSamplerState::Linear_Clamp);
	KdShaderManager::Instance().ChangeBlendState(KdBlendState::Add);

	for (int i = 0; i < kBloomNum; ++i)
	{
		KdShaderManager::Instance().m_spriteShader.DrawTex(m_bloomTex[i].get(), 0, 0, m_bloomWidth, m_bloomHeight);
	}

	KdShaderManager::Instance().UndoBlendState();
	KdShaderManager::Instance().UndoSamplerState();

	// 復元
	context->OMSetRenderTargets(1, &savedRTV, savedDSV);
	if (savedRTV) { savedRTV->Release(); }
	if (savedDSV) { savedDSV->Release(); }

	context->RSSetViewports(savedVPNum, &savedVP);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Bloom用のぼかしテクスチャをwidth x heightで(必要なら)作り直す
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPreviewPostProcess::ResizeBloomTex(int width, int height)
{
	if (width <= 0 || height <= 0) { return; }

	if (width == m_bloomWidth && height == m_bloomHeight && m_bloomTex[0]) { return; }

	m_bloomWidth = width;
	m_bloomHeight = height;

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

	for (int i = 0; i < kBloomNum; ++i)
	{
		m_bloomTex[i] = std::make_shared<KdTexture>();
		m_bloomTex[i]->Create(desc);
	}
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