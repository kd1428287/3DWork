#include "KdPostProcessShader.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// シェーダー本体の生成、定数バッファの生成
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool KdPostProcessShader::Init()
{
	// VS と InputLayout作成
	{
#include "KdPostProcessShader_VS.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateVertexShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_VS))) {
			assert(0 && "頂点シェーダー作成失敗");
			Release();
			return false;
		}

		std::vector<D3D11_INPUT_ELEMENT_DESC> layout = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,		0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,			0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateInputLayout(
			&layout[0], (UINT)layout.size(), compiledBuffer,
			sizeof(compiledBuffer), &m_inputLayout)))
		{
			assert(0 && "CreateInputLayout失敗");
			Release();
			return false;
		}
	}

	// PS 作成
	{
#include "KdPostProcessShader_PS_Blur.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(
			compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_Blur)))
		{
			assert(0 && "ピクセルシェーダー作成失敗");
			Release();

			return false;
		}

	}

	{
#include "KdPostProcessShader_PS_DoF.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(
			compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_DoF)))
		{
			assert(0 && "ピクセルシェーダー作成失敗");
			Release();

			return false;
		}
	}

	{
#include "KdPostProcessShader_PS_Bright.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(
			compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_Bright)))
		{
			assert(0 && "ピクセルシェーダー作成失敗");
			Release();

			return false;
		}
	}

	{
#include "KdPostProcessShader_PS_ColorGrade.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(
			compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_ColorGrade)))
		{
			assert(0 && "ピクセルシェーダー作成失敗(ColorGrade)");
			Release();
			return false;
		}
	}

	{
#include "KdPostProcessShader_PS_LiquidInk.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(
			compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_LiquidInk)))
		{
			assert(0 && "ピクセルシェーダー作成失敗(LiquidInk)");
			Release();
			return false;
		}
	}

	{
#include "KdPostProcessShader_PS_Distortion.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(
			compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_Distortion)))
		{
			assert(0 && "ピクセルシェーダー作成失敗(Distortion)");
			Release();
			return false;
		}
	}

	{
#include "KdPostProcessShader_PS_Surface.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(
			compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_Surface)))
		{
			assert(0 && "ピクセルシェーダー作成失敗(Surface)");
			Release();
			return false;
		}
	}

	m_cb0_BlurInfo.Create();

	m_cb0_DoFInfo.Create();

	m_cb0_BrightInfo.Create();

	m_cb0_ColorGradeInfo.Create();

	m_cb0_LiquidInfo.Create();

	m_cb0_DistortionInfo.Create();

	m_cb0_SurfaceInfo.Create();

	const std::shared_ptr<KdTexture>& backBuffer = KdDirect3D::Instance().GetBackBuffer();

	// ポストプロセス用のシーンの全描画用画像
	m_postEffectRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight(), true);

	// カラーグレード除外マスク(単チャンネルで十分。深度は不要)
	// クリア値は0.0(=グレーディング適用がデフォルト)にする
	m_colorGradeMaskRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight(), false, DXGI_FORMAT_R8_UNORM);

	// ぼかし画像
	m_blurRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight());
	m_strongBlurRTPack.CreateRenderTarget(backBuffer->GetWidth() / 2, backBuffer->GetHeight() / 2);

	// 被写界深度画像
	m_depthOfFieldRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight());

	m_brightEffectRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight());

	int lightBloomWidth = m_brightEffectRTPack.m_RTTexture->GetWidth();
	int lightBloomHeight = m_brightEffectRTPack.m_RTTexture->GetHeight();

	// 光源ぼかし画像
	for (int i = 0; i < kLightBloomNum; ++i)
	{
		m_lightBloomRTPack[i].CreateRenderTarget(lightBloomWidth, lightBloomHeight);

		lightBloomWidth /= 2;
		lightBloomHeight /= 2;
	}

	m_colorGradeRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight());

	// 液体の密度(深度共有のためフル解像度)と、そのぼかし結果(半解像度)
	m_liquidDensityRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight());
	m_liquidBlurRTPack.CreateRenderTarget(backBuffer->GetWidth() / 2, backBuffer->GetHeight() / 2);

	// ディストーション適用後の画像。衝撃波の円形補正用に縦横比も渡す
	m_distortionRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight());
	m_cb0_DistortionInfo.Work().Aspect = static_cast<float>(backBuffer->GetWidth()) / backBuffer->GetHeight();

	// 質感合成後の画像
	m_surfaceRTPack.CreateRenderTarget(backBuffer->GetWidth(), backBuffer->GetHeight());

	// 画面全体に書き込む用の頂点情報
	m_screenVert[0] = { {-1,-1,0}, {0, 1} };
	m_screenVert[1] = { {-1, 1,0}, {0, 0} };
	m_screenVert[2] = { { 1,-1,0}, {1, 1} };
	m_screenVert[3] = { { 1, 1,0}, {1, 0} };

	SetBrightThreshold(1.2f);

	SetExposure(1.0f);
	SetContrast(1.0f);
	SetSaturation(1.0f);
	SetTemperature(0.0f);
	SetTint(0.0f);


	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// シェーダー本体の解放、定数バッファの解放
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPostProcessShader::Release()
{
	KdSafeRelease(m_VS);

	KdSafeRelease(m_inputLayout);

	KdSafeRelease(m_PS_Blur);
	KdSafeRelease(m_PS_DoF);
	KdSafeRelease(m_PS_Bright);

	KdSafeRelease(m_PS_ColorGrade);
	KdSafeRelease(m_PS_LiquidInk);
	KdSafeRelease(m_PS_Distortion);
	KdSafeRelease(m_PS_Surface);


	m_cb0_BlurInfo.Release();
	m_cb0_DoFInfo.Release();
	m_cb0_BrightInfo.Release();
	m_cb0_ColorGradeInfo.Release();
	m_cb0_LiquidInfo.Release();
	m_cb0_DistortionInfo.Release();
	m_cb0_SurfaceInfo.Release();

	m_surfaceTex.reset();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPostProcessShader::Draw()
{
	// ポストエフェクトテクスチャの描画クリア
	m_postEffectRTPack.ClearTexture();

	// カラーグレード除外マスクの描画クリア(0=グレーディング適用がデフォルト)
	m_colorGradeMaskRTPack.ClearTexture(kBlackColor);

	// 光源描画テクスチャの描画クリア
	m_brightEffectRTPack.ClearTexture(kBlackColor);

	// レンダーターゲット変更(カラー本体+カラーグレード除外マスクを同時バインド：MRT)
	// ※通常のLit/UnLit用PSはSV_Target1を出力しないため、マスク側は
	//   ClearTexture()した0のまま残る。マスクを能動的に立てたい描画
	//   (パーティクル等)側だけがSV_Target1へ書き込む
	if (!m_postEffectRTChanger.ChangeRenderTargets(m_postEffectRTPack, m_colorGradeMaskRTPack))
	{
		// 失敗したらUndo
		m_postEffectRTChanger.UndoRenderTarget();
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPostProcessShader::BeginBright()
{
	// カラー本体(Bloom抽出元)+カラーグレード除外マスクを同時バインド(MRT)。
	// ※ BloomパスでもAlphaブレンドのパーティクル(m_PS_Masked使用、SV_Target1へ書き込む)が
	//   描画されうるため、Drawパス側と同様にマスクスロットを未バインドのまま残さない。
	//   深度バッファ・ビューポートは従来通りm_postEffectRTPack/m_brightEffectRTPack側のものを使う
	//  (ChangeRenderTargets(colorRTPack, maskRTPack)の2引数版はcolorRTPack自身のZBuffer/ViewPortを
	//   使ってしまい、このBloomパスが必要とする深度・ビューポートの組み合わせと合わないため使わない)
	ID3D11RenderTargetView* rtvs[2] =
	{
		m_brightEffectRTPack.m_RTTexture->WorkRTView(),
		m_colorGradeMaskRTPack.m_RTTexture->WorkRTView()
	};

	ID3D11DepthStencilView* pDSV = m_postEffectRTPack.m_ZBuffer ? m_postEffectRTPack.m_ZBuffer->WorkDSView() : nullptr;

	if (!m_brightRTChanger.ChangeRenderTargets(rtvs, 2, pDSV, &m_brightEffectRTPack.m_viewPort))
	{
		m_brightRTChanger.UndoRenderTarget();
	}

	KdShaderManager::Instance().ChangeBlendState(KdBlendState::Add);

	KdShaderManager::Instance().ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);
}

void KdPostProcessShader::EndBright()
{
	KdShaderManager::Instance().UndoDepthStencilState();

	KdShaderManager::Instance().UndoBlendState();

	m_brightRTChanger.UndoRenderTarget();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 液体の密度RTへ切り替える(深度はシーンのものを共有。BeginBrightと同じ構成)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPostProcessShader::BeginLiquid(bool useSceneDepth)
{
	m_liquidDensityRTPack.ClearTexture(kBlackColor);

	ID3D11RenderTargetView* rtv = m_liquidDensityRTPack.m_RTTexture->WorkRTView();

	ID3D11DepthStencilView* pDSV = (useSceneDepth && m_postEffectRTPack.m_ZBuffer) ? m_postEffectRTPack.m_ZBuffer->WorkDSView() : nullptr;

	// 密度RT全体をNDC基準の画像として使う(出力先のサイズが違っても合成時のUVが一致する)
	if (!m_liquidRTChanger.ChangeRenderTargets(&rtv, 1, pDSV, &m_liquidDensityRTPack.m_viewPort))
	{
		m_liquidRTChanger.UndoRenderTarget();
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 密度RTを元のRTへ戻し、ブラー→しきい値でシーンRTへ合成する
//	合成先はカラー+カラーグレード除外マスクのMRT(AlphaMasked)。深度テストは密度描画時に済んでいる
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPostProcessShader::EndLiquid(LiquidStyle style)
{
	m_liquidRTChanger.UndoRenderTarget();

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();
	if (!DevCon) { return; }

	// スタイルごとの合成PS(血・水を足す時はここへcaseを追加する)
	ID3D11PixelShader* pPS = nullptr;
	switch (style)
	{
	case LiquidStyle::Ink: pPS = m_PS_LiquidInk; break;
	}
	if (!pPS) { return; }

	// 密度を半解像度へぼかす(ダウンサンプルも兼ねる)
	SetBlurToDevice();
	GenerateBlurTexture(m_liquidDensityRTPack.m_RTTexture, m_liquidBlurRTPack.m_RTTexture, m_liquidBlurRTPack.m_viewPort, kBlurSamplingRadius);

	m_cb0_LiquidInfo.Write();
	DevCon->PSSetConstantBuffers(0, 1, m_cb0_LiquidInfo.GetAddress());

	KdShaderManager& shaderMgr = KdShaderManager::Instance();
	shaderMgr.SetPixelShader(pPS);

	shaderMgr.ChangeSamplerState(KdSamplerState::Linear_Clamp);
	shaderMgr.ChangeBlendState(KdBlendState::AlphaMasked);
	shaderMgr.ChangeDepthStencilState(KdDepthStencilState::ZDisable);

	// 出力先を切り替えず、現在のシーンRTへ全画面描画する
	DrawTexture(&m_liquidBlurRTPack.m_RTTexture, 1, nullptr, nullptr);

	shaderMgr.UndoDepthStencilState();
	shaderMgr.UndoBlendState();
	shaderMgr.UndoSamplerState();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 衝撃波の発生(画面UV指定)。空きが無ければ最も進行した1つを上書きする
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPostProcessShader::AddShockwave(const Math::Vector2& centerUV, float duration, float maxRadius, float strength)
{
	if (duration <= 0.0f) { return; }

	ShockwaveInstance* pSlot = nullptr;
	float maxProgress = -1.0f;

	for (ShockwaveInstance& s : m_shockwaves)
	{
		if (!s.Active) { pSlot = &s; break; }

		float progress = s.Age / s.Duration;
		if (progress > maxProgress)
		{
			maxProgress = progress;
			pSlot = &s;
		}
	}

	pSlot->Center = centerUV;
	pSlot->Age = 0.0f;
	pSlot->Duration = duration;
	pSlot->MaxRadius = maxRadius;
	pSlot->Strength = strength;
	pSlot->Active = true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 衝撃波の発生(ワールド座標指定)。カメラの背後なら何もしない
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPostProcessShader::AddShockwaveWorld(const Math::Vector3& worldPos, float duration, float maxRadius, float strength)
{
	const KdShaderManager::cbCamera& cam = KdShaderManager::Instance().GetCameraCB();

	Math::Vector4 clip = Math::Vector4::Transform(Math::Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f), cam.mView * cam.mProj);
	if (clip.w <= 0.0f) { return; }

	// NDC(-1〜1、Yは上が正)→画面UV(0〜1、Vは下が正)
	Math::Vector2 uv;
	uv.x = clip.x / clip.w * 0.5f + 0.5f;
	uv.y = 0.5f - clip.y / clip.w * 0.5f;

	AddShockwave(uv, duration, maxRadius, strength);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 経過時間から各衝撃波の半径と強さを算出して定数バッファへ反映する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPostProcessShader::UpdateDistortion(float deltaTime)
{
	cbDistortionInfo& cb = m_cb0_DistortionInfo.Work();

	int num = 0;

	for (ShockwaveInstance& s : m_shockwaves)
	{
		if (!s.Active) { continue; }

		s.Age += deltaTime;

		float t = s.Age / s.Duration;
		if (t >= 1.0f)
		{
			s.Active = false;
			continue;
		}

		// 半径は最初に速く広がって減速、強さは2乗で素早く減衰
		float inv = 1.0f - t;
		float radius = s.MaxRadius * (1.0f - inv * inv * inv);
		float strength = s.Strength * inv * inv;

		cb.Shockwave[num] = Math::Vector4(s.Center.x, s.Center.y, radius, strength);
		++num;
	}

	cb.ShockwaveNum = num;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// カラーグレード済みの画像へ衝撃波の歪みと色収差を適用して m_distortionRTPack に描画
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPostProcessShader::DistortionProcess()
{
	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();
	if (!DevCon) { return; }

	m_cb0_DistortionInfo.Write();
	DevCon->PSSetConstantBuffers(0, 1, m_cb0_DistortionInfo.GetAddress());

	KdShaderManager& shaderMgr = KdShaderManager::Instance();
	if (shaderMgr.SetVertexShader(m_VS))
	{
		DevCon->IASetInputLayout(m_inputLayout);
	}
	shaderMgr.SetPixelShader(m_PS_Distortion);

	shaderMgr.ChangeSamplerState(KdSamplerState::Linear_Clamp);

	DrawTexture(&m_colorGradeRTPack.m_RTTexture, 1, m_distortionRTPack.m_RTTexture, &m_distortionRTPack.m_viewPort);

	shaderMgr.UndoSamplerState();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 質感テクスチャを明暗として合成して m_surfaceRTPack に描画
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPostProcessShader::SurfaceProcess(const std::shared_ptr<KdTexture>& srcTex)
{
	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();
	if (!DevCon) { return; }

	m_cb0_SurfaceInfo.Write();
	DevCon->PSSetConstantBuffers(0, 1, m_cb0_SurfaceInfo.GetAddress());

	KdShaderManager& shaderMgr = KdShaderManager::Instance();
	if (shaderMgr.SetVertexShader(m_VS))
	{
		DevCon->IASetInputLayout(m_inputLayout);
	}
	shaderMgr.SetPixelShader(m_PS_Surface);

	shaderMgr.ChangeSamplerState(KdSamplerState::Linear_Clamp);

	// t0=シーン画像 t1=質感
	std::shared_ptr<KdTexture> srcTexList[2] = { srcTex, m_surfaceTex };
	DrawTexture(srcTexList, 2, m_surfaceRTPack.m_RTTexture, &m_surfaceRTPack.m_viewPort);

	shaderMgr.UndoSamplerState();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 一定間隔で質感の位置をずらす(画面に貼り付いて見えるのを防ぐ)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPostProcessShader::UpdateSurface(float deltaTime)
{
	if (m_surfaceJitterInterval <= 0.0f) { return; }

	m_surfaceJitterTimer += deltaTime;
	if (m_surfaceJitterTimer < m_surfaceJitterInterval) { return; }
	m_surfaceJitterTimer = 0.0f;

	cbSurfaceInfo& cb = m_cb0_SurfaceInfo.Work();
	cb.Offset.x = static_cast<float>(rand()) / RAND_MAX;
	cb.Offset.y = static_cast<float>(rand()) / RAND_MAX;
}

void KdPostProcessShader::PostEffectProcess()
{
	m_postEffectRTChanger.UndoRenderTarget();

	LightBloomProcess();
	BlurProcess();
	DepthOfFieldProcess();
	ColorGradeProcess();

	// 衝撃波が無い間は全画面コピーを省いてカラーグレード結果をそのまま使う
	std::shared_ptr<KdTexture> finalTex = m_colorGradeRTPack.m_RTTexture;

	if (m_cb0_DistortionInfo.Get().ShockwaveNum > 0)
	{
		DistortionProcess();
		finalTex = m_distortionRTPack.m_RTTexture;
	}

	// 質感未設定または強さ0の間はパスごと省く
	if (m_surfaceTex && m_cb0_SurfaceInfo.Get().Intensity > 0.0f)
	{
		SurfaceProcess(finalTex);
		finalTex = m_surfaceRTPack.m_RTTexture;
	}

	KdShaderManager::Instance().m_spriteShader.DrawTex(finalTex.get(), 0, 0);
}

void KdPostProcessShader::LightBloomProcess()
{
	SetBrightToDevice();

	KdShaderManager::Instance().ChangeBlendState(KdBlendState::Add);

	// 高輝度抽出
	DrawTexture(&m_postEffectRTPack.m_RTTexture, 1, m_brightEffectRTPack.m_RTTexture, &m_brightEffectRTPack.m_viewPort);

	KdShaderManager::Instance().UndoBlendState();

	// LightBloom画像の作成
	SetBlurToDevice();

	std::shared_ptr<KdTexture> srcRTTex = m_brightEffectRTPack.m_RTTexture;

	for (int i = 0; i < kLightBloomNum; ++i)
	{
		GenerateBlurTexture(srcRTTex, m_lightBloomRTPack[i].m_RTTexture, m_lightBloomRTPack[i].m_viewPort, kBlurSamplingRadius);

		srcRTTex = m_lightBloomRTPack[i].m_RTTexture;
	}

	KdRenderTargetChanger RTChanger;
	RTChanger.ChangeRenderTarget(m_postEffectRTPack);

	KdShaderManager::Instance().ChangeSamplerState(KdSamplerState::Linear_Clamp);

	KdShaderManager::Instance().ChangeBlendState(KdBlendState::Add);

	// 光源ぼかし画像の合成
	for (int i = 0; i < kLightBloomNum; ++i)
	{
		KdShaderManager::Instance().m_spriteShader.DrawTex(m_lightBloomRTPack[i].m_RTTexture.get(), 0, 0, m_postEffectRTPack.m_RTTexture->GetWidth(), m_postEffectRTPack.m_RTTexture->GetHeight());
	}

	RTChanger.UndoRenderTarget();

	KdShaderManager::Instance().UndoBlendState();

	KdShaderManager::Instance().UndoSamplerState();
}

void KdPostProcessShader::BlurProcess()
{
	SetBlurToDevice();

	GenerateBlurTexture(m_postEffectRTPack.m_RTTexture, m_blurRTPack.m_RTTexture, m_blurRTPack.m_viewPort, kBlurSamplingRadius);

	GenerateBlurTexture(m_blurRTPack.m_RTTexture, m_strongBlurRTPack.m_RTTexture, m_strongBlurRTPack.m_viewPort, kBlurSamplingRadius);
}

void KdPostProcessShader::DepthOfFieldProcess()
{
	SetDoFToDevice();

	std::shared_ptr<KdTexture> srcTexList[5] =
	{
		m_postEffectRTPack.m_RTTexture,
		m_blurRTPack.m_RTTexture,
		m_strongBlurRTPack.m_RTTexture,
		m_postEffectRTPack.m_ZBuffer,
		m_colorGradeMaskRTPack.m_RTTexture		// 追加：DoFブラー除外マスク(カラーグレード除外マスクを共用)
	};

	DrawTexture(srcTexList, 5, m_depthOfFieldRTPack.m_RTTexture, &m_depthOfFieldRTPack.m_viewPort);
}

void KdPostProcessShader::ColorGradeProcess()
{
	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();
	if (DevCon)
	{
		// 定数バッファ書き込み
		m_cb0_ColorGradeInfo.Write();
		DevCon->PSSetConstantBuffers(0, 1, m_cb0_ColorGradeInfo.GetAddress());
	}

	// デバイスにシェーダーセット
	KdShaderManager& shaderMgr = KdShaderManager::Instance();
	if (shaderMgr.SetVertexShader(m_VS))
	{
		DevCon->IASetInputLayout(m_inputLayout);
	}
	shaderMgr.SetPixelShader(m_PS_ColorGrade);

	// サンプラーステート設定
	shaderMgr.ChangeSamplerState(KdSamplerState::Linear_Clamp);

	// DoF結果(t0)+カラーグレード除外マスク(t1)を入力として、m_colorGradeRTPack に描画
	std::shared_ptr<KdTexture> srcTexList[2] =
	{
		m_depthOfFieldRTPack.m_RTTexture,
		m_colorGradeMaskRTPack.m_RTTexture
	};

	DrawTexture(srcTexList, 2, m_colorGradeRTPack.m_RTTexture, &m_colorGradeRTPack.m_viewPort);

	shaderMgr.UndoSamplerState();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エディタプレビュー等、本編以外の任意サイズテクスチャに対してカラーグレードのみを適用する
//	・本編のColorGradeProcess()と処理内容は同一だが、対象テクスチャを引数化したもの
//	・m_depthOfFieldRTPack/m_colorGradeMaskRTPack/m_colorGradeRTPack(本編専用・固定サイズ)には
//	  一切触れないため、本編のポストプロセスパイプラインへの影響は無い
//	・DrawTexture()内部で自前のKdRenderTargetChangerを使って呼び出し前後のRT/ビューポートを
//	  自動的に退避・復元するため、呼び出し側で特別な後始末は不要
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdPostProcessShader::ApplyColorGradeOnly(std::shared_ptr<KdTexture> srcColor, std::shared_ptr<KdTexture> srcMask,
	std::shared_ptr<KdTexture> dstColor, D3D11_VIEWPORT* pVP)
{
	if (!srcColor || !srcMask || !dstColor) { return; }

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();
	if (!DevCon) { return; }

	// 定数バッファ書き込み(本編と共通のカラーグレードパラメータをそのまま使う)
	m_cb0_ColorGradeInfo.Write();
	DevCon->PSSetConstantBuffers(0, 1, m_cb0_ColorGradeInfo.GetAddress());

	KdShaderManager& shaderMgr = KdShaderManager::Instance();
	if (shaderMgr.SetVertexShader(m_VS))
	{
		DevCon->IASetInputLayout(m_inputLayout);
	}
	shaderMgr.SetPixelShader(m_PS_ColorGrade);

	shaderMgr.ChangeSamplerState(KdSamplerState::Linear_Clamp);

	std::shared_ptr<KdTexture> srcTexList[2] = { srcColor, srcMask };

	DrawTexture(srcTexList, 2, dstColor, pVP);

	shaderMgr.UndoSamplerState();
}

void KdPostProcessShader::CreateBlurOffsetList(std::vector<Math::Vector3>& dstInfo, const std::shared_ptr<KdTexture>& spSrcTex, int samplingRadius, const Math::Vector2& dir)
{
	Math::Vector2 blurDir = dir;
	blurDir.Normalize();

	// 両サイドのサンプリング回数 ＋ サンプル開始中央のピクセル
	int totalSamplingNum = samplingRadius * 2 + 1;

	// サンプリングするテクセルのオフセット値
	Math::Vector2 texelSize;
	texelSize.x = 1.0f / spSrcTex->GetWidth();
	texelSize.y = 1.0f / spSrcTex->GetHeight();

	dstInfo.resize(totalSamplingNum);

	float totalWeight = 0;
	for (int i = 0; i < totalSamplingNum; ++i)
	{
		int samplingOffset = i - samplingRadius;
		dstInfo[i].x = blurDir.x * (samplingOffset * texelSize.x);
		dstInfo[i].y = blurDir.y * (samplingOffset * texelSize.y);

		// 中心のピクセルのウェイトが大きくなる計算
		float weight = exp(-(samplingOffset * samplingOffset) / 18.0f);

		// サンプリングする各ピクセルに重みをつける
		dstInfo[i].z = weight;
		totalWeight += weight;
	}

	// ウェイトを全体のウェイトから割り算し、各ピクセルのウェイトの意味を割合に置き換える
	// 全部足して1になるように数値を調整する
	for (int i = 0; i < totalSamplingNum; ++i)
	{
		dstInfo[i].z /= totalWeight;
	}
}

void KdPostProcessShader::GenerateBlurTexture(std::shared_ptr<KdTexture>& spSrcTex, std::shared_ptr<KdTexture>& spDstTex, D3D11_VIEWPORT& VP, int blurRadius)
{
	KdShaderManager::Instance().ChangeSamplerState(KdSamplerState::Linear_Clamp);

	KdRenderTargetPack tmpBlurRTPack;
	tmpBlurRTPack.CreateRenderTarget(spDstTex->GetWidth(), spDstTex->GetHeight());

	// 横にぼかす
	std::vector<Math::Vector3> horizontalBlurInfo;
	CreateBlurOffsetList(horizontalBlurInfo, spDstTex, blurRadius, { 1.0f, 0 });
	SetBlurInfo(horizontalBlurInfo);

	DrawTexture(&spSrcTex, 1, tmpBlurRTPack.m_RTTexture, &tmpBlurRTPack.m_viewPort);

	// 横にぼかした画像を更に縦にぼかす
	std::vector<Math::Vector3> verticalBlurInfo;
	CreateBlurOffsetList(verticalBlurInfo, spDstTex, blurRadius, { 0, 1.0f });
	SetBlurInfo(verticalBlurInfo);

	DrawTexture(&tmpBlurRTPack.m_RTTexture, 1, spDstTex, &VP);

	KdShaderManager::Instance().UndoSamplerState();
}

void KdPostProcessShader::DrawTexture(std::shared_ptr<KdTexture>* spSrcTex, int srcTexSize, std::shared_ptr<KdTexture> spDstTex, D3D11_VIEWPORT* pVP)
{
	if (!spSrcTex) { return; }

	KdRenderTargetChanger RTChanger;

	if (spDstTex)
	{
		RTChanger.ChangeRenderTarget(spDstTex, nullptr, pVP);
	}

	ID3D11DeviceContext* pDevCon = KdDirect3D::Instance().WorkDevContext();

	// SRVのセット
	for (int i = 0; i < srcTexSize; ++i)
	{
		pDevCon->PSSetShaderResources(i, 1, spSrcTex[i]->WorkSRViewAddress());
	}

	// テクスチャーの描画
	KdDirect3D::Instance().DrawVertices(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 4, &m_screenVert[0], sizeof(Vertex));

	// SRVの解放
	ID3D11ShaderResourceView* nullSRV = nullptr;

	for (int i = 0; i < srcTexSize; ++i)
	{
		pDevCon->PSSetShaderResources(i, 1, &nullSRV);
	}

	RTChanger.UndoRenderTarget();
}

void KdPostProcessShader::SetBlurInfo(const std::shared_ptr<KdTexture>& spSrcTex, int samplingRadius, const Math::Vector2& dir)
{
	std::vector<Math::Vector3> blurOffsetList;

	CreateBlurOffsetList(blurOffsetList, spSrcTex, samplingRadius, dir);

	SetBlurInfo(blurOffsetList);
}

void KdPostProcessShader::SetBlurInfo(const std::vector<Math::Vector3>& srcInfo)
{
	KdPostProcessShader::cbBlur& blurInfo = m_cb0_BlurInfo.Work();

	blurInfo.SamplingNum = (signed)srcInfo.size();

	if (blurInfo.SamplingNum > kMaxSampling)
	{
		assert(0 && "サンプリング指定回数が上限を超えています。");

		blurInfo.SamplingNum = 0;

		return;
	}

	for (int i = 0; i < blurInfo.SamplingNum; ++i)
	{
		blurInfo.Info[i].x = srcInfo[i].x;
		blurInfo.Info[i].y = srcInfo[i].y;
		blurInfo.Info[i].z = srcInfo[i].z;
	}

	m_cb0_BlurInfo.Write();
}

void KdPostProcessShader::SetBlurToDevice()
{
	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();
	if (!DevCon) { return; }

	m_cb0_BlurInfo.Write();

	KdDirect3D::Instance().WorkDevContext()->PSSetConstantBuffers(0, 1, m_cb0_BlurInfo.GetAddress());

	KdShaderManager& shaderMgr = KdShaderManager::Instance();

	if (shaderMgr.SetVertexShader(m_VS))
	{
		DevCon->IASetInputLayout(m_inputLayout);
	}

	shaderMgr.SetPixelShader(m_PS_Blur);
}

void KdPostProcessShader::SetDoFToDevice()
{
	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();
	if (!DevCon) { return; }

	m_cb0_DoFInfo.Write();

	KdDirect3D::Instance().WorkDevContext()->PSSetConstantBuffers(0, 1, m_cb0_DoFInfo.GetAddress());

	KdShaderManager& shaderMgr = KdShaderManager::Instance();

	if (shaderMgr.SetVertexShader(m_VS))
	{
		DevCon->IASetInputLayout(m_inputLayout);
	}

	shaderMgr.SetPixelShader(m_PS_DoF);
}

void KdPostProcessShader::SetBrightToDevice()
{
	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();
	if (!DevCon) { return; }

	m_cb0_BrightInfo.Write();

	KdDirect3D::Instance().WorkDevContext()->PSSetConstantBuffers(0, 1, m_cb0_BrightInfo.GetAddress());

	KdShaderManager& shaderMgr = KdShaderManager::Instance();

	if (shaderMgr.SetVertexShader(m_VS))
	{
		DevCon->IASetInputLayout(m_inputLayout);
	}

	shaderMgr.SetPixelShader(m_PS_Bright);
}