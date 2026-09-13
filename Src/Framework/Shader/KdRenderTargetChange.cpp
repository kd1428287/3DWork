#include "KdRenderTargetChange.h"

//===========================================
//
// レンダーターゲットのパッケージ　RTView-DSView-ViewPort
//
//===========================================

void KdRenderTargetPack::CreateRenderTarget(int width, int height, bool needDSV, DXGI_FORMAT format, D3D11_VIEWPORT* pVP)
{
	m_RTTexture = std::make_shared<KdTexture>();
	m_RTTexture->CreateRenderTarget(width, height, format);

	if (needDSV)
	{
		m_ZBuffer = std::make_shared<KdTexture>();
		m_ZBuffer->CreateDepthStencil(width, height);
	}

	SetViewPort(pVP);
}

void KdRenderTargetPack::SetRenderTarget(std::shared_ptr<KdTexture> RTT, std::shared_ptr<KdTexture> DST, D3D11_VIEWPORT* pVP)
{
	m_RTTexture = RTT;
	m_ZBuffer = DST;

	SetViewPort(pVP);
}

void KdRenderTargetPack::SetViewPort(D3D11_VIEWPORT* pVP)
{
	if (pVP)
	{
		m_viewPort = *pVP;
	}
	else
	{
		if (!m_RTTexture) { return; }

		// レンダーターゲットテクスチャから生成
		m_viewPort =
		{
			0.0f,
			0.0f,
			static_cast<float>(m_RTTexture->GetWidth()),
			static_cast<float>(m_RTTexture->GetHeight()),
			0.0f,
			1.0f
		};
	}
}

void KdRenderTargetPack::ClearTexture(const Math::Color& fillColor)
{
	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

	if (m_RTTexture)
	{
		// テクスチャの描画クリア
		DevCon->ClearRenderTargetView(m_RTTexture->WorkRTView(), fillColor);
	}

	if (m_ZBuffer)
	{
		DevCon->ClearDepthStencilView(m_ZBuffer->WorkDSView(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}
}


//===========================================
//
// レンダーターゲット変更クラス
//
//===========================================

bool KdRenderTargetChanger::Validate(ID3D11RenderTargetView* const* pRTVs, UINT numRTV)
{
	if (!pRTVs || numRTV == 0 || numRTV > kMaxSimultaneousRTV)
	{
		assert(0 && "変更先のRenderTargetの指定が不正です");
		return false;
	}

	for (UINT i = 0; i < numRTV; ++i)
	{
		if (!pRTVs[i])
		{
			assert(0 && "変更先のRenderTargetがありません");
			return false;
		}
	}

	if (m_savedRTVCount != 0)
	{
		assert(0 && "既にRenderTargetを変更済みです");
		return false;
	}

	return true;
}

bool KdRenderTargetChanger::ChangeRenderTarget(ID3D11RenderTargetView* pRTV,
	ID3D11DepthStencilView* pDSV, D3D11_VIEWPORT* pVP)
{
	// 単一RTVも内部的にはMRT用の実装(1枚版)へ委譲する
	return ChangeRenderTargets(&pRTV, 1, pDSV, pVP);
}

bool KdRenderTargetChanger::ChangeRenderTarget(std::shared_ptr<KdTexture> RTT,
	std::shared_ptr<KdTexture> DST, D3D11_VIEWPORT* pVP)
{
	if (!RTT) { return false; }

	ID3D11DepthStencilView* pDSV = nullptr;

	if (DST)
	{
		pDSV = DST->WorkDSView();
	}

	return ChangeRenderTarget(RTT->WorkRTView(), pDSV, pVP);
}

bool KdRenderTargetChanger::ChangeRenderTarget(KdRenderTargetPack& RTPack)
{
	return ChangeRenderTarget(RTPack.m_RTTexture, RTPack.m_ZBuffer, &RTPack.m_viewPort);
}

bool KdRenderTargetChanger::ChangeRenderTargets(ID3D11RenderTargetView* const* pRTVs, UINT numRTV,
	ID3D11DepthStencilView* pDSV, D3D11_VIEWPORT* pVP)
{
	if (!Validate(pRTVs, numRTV)) { return false; }

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

	// 情報保存：これから何枚バインドするか(numRTV)に関わらず、
	// 常に最大枚数(kMaxSimultaneousRTV)ぶん退避しておく。
	// ※OMSetRenderTargetsはNumViewsで指定した数以外の全スロットを暗黙的に
	//   解除する仕様のため、例えば「現在2枚(カラー+マスク)バインドされている状態から
	//   シャドウマップ用に1枚だけへ一時的に切り替える」ような場合、numRTV=1でしか
	//   退避しないと2枚目の情報が失われ、Undo時に2枚目が未バインドのまま
	//   戻ってしまう(実際にこれが今回の警告の原因だった)
	DevCon->OMGetRenderTargets(kMaxSimultaneousRTV, m_saveRTVs, &m_saveDSV);
	m_savedRTVCount = kMaxSimultaneousRTV;

	// レンダーターゲット切替 ----- ----- ----- ----- -----
	DevCon->OMSetRenderTargets(numRTV, pRTVs, pDSV);

	if (pVP)
	{
		DevCon->RSGetViewports(&m_numVP, &m_saveVP);
		DevCon->RSSetViewports(1, pVP);
		m_changeVP = true;
	}

	return true;
}

bool KdRenderTargetChanger::ChangeRenderTargets(KdRenderTargetPack& colorRTPack, KdRenderTargetPack& maskRTPack)
{
	if (!colorRTPack.m_RTTexture || !maskRTPack.m_RTTexture) { return false; }

	ID3D11RenderTargetView* rtvs[2] =
	{
		colorRTPack.m_RTTexture->WorkRTView(),
		maskRTPack.m_RTTexture->WorkRTView()
	};

	ID3D11DepthStencilView* pDSV = colorRTPack.m_ZBuffer ? colorRTPack.m_ZBuffer->WorkDSView() : nullptr;

	return ChangeRenderTargets(rtvs, 2, pDSV, &colorRTPack.m_viewPort);
}

void KdRenderTargetChanger::UndoRenderTarget()
{
	// 復帰すべきレンダーターゲットが存在しない
	if (m_savedRTVCount == 0) { return; }

	KdDirect3D::Instance().WorkDevContext()->OMSetRenderTargets(m_savedRTVCount, m_saveRTVs, m_saveDSV);

	if (m_changeVP)
	{
		KdDirect3D::Instance().WorkDevContext()->RSSetViewports(1, &m_saveVP);
	}

	for (UINT i = 0; i < m_savedRTVCount; ++i)
	{
		KdSafeRelease(m_saveRTVs[i]);
	}
	m_savedRTVCount = 0;

	KdSafeRelease(m_saveDSV);

	m_changeVP = false;
}

void KdRenderTargetChanger::Release()
{
	for (UINT i = 0; i < kMaxSimultaneousRTV; ++i)
	{
		KdSafeRelease(m_saveRTVs[i]);
	}
	m_savedRTVCount = 0;

	KdSafeRelease(m_saveDSV);
}