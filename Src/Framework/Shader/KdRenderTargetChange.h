#pragma once

//===========================================
//
// レンダーターゲット変更
//
//===========================================
struct KdRenderTargetPack
{
	KdRenderTargetPack() {}

	void CreateRenderTarget(int width, int height, bool needDSV = false, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_VIEWPORT* pVP = nullptr);
	void SetRenderTarget(std::shared_ptr<KdTexture> RTT, std::shared_ptr<KdTexture> DST = nullptr, D3D11_VIEWPORT* pVP = nullptr);

	void SetViewPort(D3D11_VIEWPORT* pVP);

	void ClearTexture(const Math::Color& fillColor = kGrayColor);

	std::shared_ptr<KdTexture> m_RTTexture = nullptr;
	std::shared_ptr<KdTexture> m_ZBuffer = nullptr;
	D3D11_VIEWPORT m_viewPort = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
};

struct KdRenderTargetChanger
{
	// 同時にバインドするRTVの最大数
	// (現状の用途：カラー本体+カラーグレード除外マスクの2枚まで。
	//  それ以上のMRTが必要になったらここを増やすだけでよい)
	static const UINT kMaxSimultaneousRTV = 2;

	~KdRenderTargetChanger() { Release(); }

	ID3D11RenderTargetView* m_saveRTVs[kMaxSimultaneousRTV] = {};
	UINT					m_savedRTVCount = 0;
	ID3D11DepthStencilView* m_saveDSV = nullptr;
	D3D11_VIEWPORT			m_saveVP = {};
	UINT					m_numVP = 1;
	bool					m_changeVP = false;

	bool Validate(ID3D11RenderTargetView* const* pRTVs, UINT numRTV);

	//----------------------------------------
	// 単一レンダーターゲットへの変更(既存互換。内部的にはChangeRenderTargets(1枚版)へ委譲)
	//----------------------------------------
	bool ChangeRenderTarget(ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDSV = nullptr, D3D11_VIEWPORT* pVP = nullptr);
	bool ChangeRenderTarget(std::shared_ptr<KdTexture> RTT, std::shared_ptr<KdTexture> DST = nullptr, D3D11_VIEWPORT* pVP = nullptr);
	bool ChangeRenderTarget(KdRenderTargetPack& RTPack);

	//----------------------------------------
	// 複数レンダーターゲット(MRT)への変更
	//----------------------------------------
	// ・pRTVs		… バインドしたいRTVの配列(先頭からSV_Target0, SV_Target1, ...に対応)
	// ・numRTV		… 配列の要素数(kMaxSimultaneousRTV以下)
	bool ChangeRenderTargets(ID3D11RenderTargetView* const* pRTVs, UINT numRTV, ID3D11DepthStencilView* pDSV = nullptr, D3D11_VIEWPORT* pVP = nullptr);

	// カラー用・マスク用の2枚のKdRenderTargetPackを同時にバインドする便利オーバーロード。
	// 深度バッファ・ビューポートはcolorRTPack側のものを使用する。
	bool ChangeRenderTargets(KdRenderTargetPack& colorRTPack, KdRenderTargetPack& maskRTPack);

	void UndoRenderTarget();

	void Release();
};