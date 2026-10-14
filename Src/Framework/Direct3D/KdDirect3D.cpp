#include "Framework/KdFramework.h"

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

void KdDirect3D::CopyViewportInfo(Math::Viewport& out) const
{
	UINT numVPs = 1;
	D3D11_VIEWPORT vp;
	m_pDeviceContext->RSGetViewports(&numVPs, &vp);
	out = vp;
}

void KdDirect3D::ClientToWorld(const POINT& screenPos, float porjZ, Math::Vector3& dst, const Math::Matrix& mCam, const Math::Matrix& mProj)
{
	porjZ = std::clamp(porjZ, 0.0f, 1.0f);

	// 各行列の逆行列を算出
	Math::Matrix invView = mCam;
	Math::Matrix invPrj = mProj.Invert();

	Math::Matrix mVP;
	mVP._11 = m_windowWidth * 0.5f;
	mVP._22 = -m_windowHeight * 0.5f;
	mVP._41 = m_windowWidth * 0.5f;
	mVP._42 = m_windowHeight * 0.5f;

	Math::Matrix invViewport = mVP.Invert();

	// 2D→3D変換用行列合成
	Math::Matrix convertMat = invViewport * invPrj * invView;

	Math::Vector3::Transform({ (float)screenPos.x, (float)screenPos.y, porjZ }, convertMat, dst);
}

void KdDirect3D::WorldToClient(const Math::Vector3& srcWorld, POINT& dst, const Math::Matrix& mCam, const Math::Matrix& mProj)
{
	Math::Matrix mVP;
	mVP._11 = m_windowWidth * 0.5f;
	mVP._22 = -m_windowHeight * 0.5f;
	mVP._41 = m_windowWidth * 0.5f;
	mVP._42 = m_windowHeight * 0.5f;

	Math::Matrix convertMat = mCam.Invert() * mProj * mVP;

	Math::Vector3 screenPos = Math::Vector3::Transform(srcWorld, convertMat);

	dst.x = static_cast<int>(screenPos.x);
	dst.y = static_cast<int>(screenPos.y);
}

bool KdDirect3D::Init(HWND hWnd, int w, int h, bool deviceDebug, std::string& errMsg)
{
	HRESULT hr;

	//=====================================================
	// ファクトリー作成(ビデオ グラフィックの設定の列挙や指定に使用されるオブジェクト)
	//=====================================================
	IDXGIFactory* factory = nullptr;
	hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory));
	if (FAILED(hr)) {
		errMsg = "ファクトリー作成失敗";

		Release();
		return false;
	}

	//=====================================================
	//デバイス生成(主にリソース作成時に使用するオブジェクト)
	//=====================================================
	UINT creationFlags = 0;

	if (deviceDebug) {
		// Direct3Dのデバッグを有効にする(すごく重いが細かいエラーがわかる)
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	}

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,	// Direct3D 11.1  ShaderModel 5
		D3D_FEATURE_LEVEL_11_0,	// Direct3D 11    ShaderModel 5
		D3D_FEATURE_LEVEL_10_1,	// Direct3D 10.1  ShaderModel 4
		D3D_FEATURE_LEVEL_10_0,	// Direct3D 10.0  ShaderModel 4
		D3D_FEATURE_LEVEL_9_3,	// Direct3D 9.3   ShaderModel 3
		D3D_FEATURE_LEVEL_9_2,	// Direct3D 9.2   ShaderModel 3
		D3D_FEATURE_LEVEL_9_1,	// Direct3D 9.1   ShaderModel 3
	};

	// 使用GPU選択（グラフィック専用メモリの容量の多い物を優先
	IDXGIAdapter* pSelectAdapter = nullptr;
	SIZE_T maxVideoMemory = 0;
	for (UINT index = 0; 1; ++index)
	{
		IDXGIAdapter* adapter = nullptr;
		HRESULT ret = factory->EnumAdapters(index, &adapter);

		if (ret == DXGI_ERROR_NOT_FOUND) { break; }
		DXGI_ADAPTER_DESC desc = {};
		adapter->GetDesc(&desc);

		if (maxVideoMemory < desc.DedicatedVideoMemory)
		{
			maxVideoMemory = desc.DedicatedVideoMemory;
			pSelectAdapter = adapter;
		}
	}

	// デバイスとでデバイスコンテキストを作成
	D3D_FEATURE_LEVEL futureLevel;
	hr = D3D11CreateDevice(
		pSelectAdapter,
		D3D_DRIVER_TYPE_UNKNOWN,
		nullptr,
		creationFlags,
		featureLevels,
		_countof(featureLevels),
		D3D11_SDK_VERSION,
		&m_pDevice,
		&futureLevel,
		&m_pDeviceContext);

	if FAILED(hr)
	{
		errMsg = "Direct3D11デバイス作成失敗";

		Release();
		return false;
	}

	//=====================================================
	// スワップチェイン作成(フロントバッファに表示可能なバックバッファを持つもの)
	//=====================================================
	DXGI_SWAP_CHAIN_DESC DXGISwapChainDesc = {};		// スワップチェーンの設定データ
	DXGISwapChainDesc.BufferDesc.Width = w;
	DXGISwapChainDesc.BufferDesc.Height = h;
	DXGISwapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
	DXGISwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	DXGISwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGISwapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	DXGISwapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;// DXGI_MODE_SCALING_CENTERED;	// DXGI_MODE_SCALING_UNSPECIFIED
	DXGISwapChainDesc.SampleDesc.Count = 1;
	DXGISwapChainDesc.SampleDesc.Quality = 0;
	DXGISwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
	DXGISwapChainDesc.BufferCount = 2;
	DXGISwapChainDesc.OutputWindow = hWnd;
	DXGISwapChainDesc.Windowed = TRUE;
	DXGISwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	DXGISwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	if (FAILED(factory->CreateSwapChain(m_pDevice, &DXGISwapChainDesc, &m_pGISwapChain))) {
		errMsg = "スワップチェイン作成失敗";

		Release();
		return false;
	}
	// Alt+Enterによる排他フルスクリーン切替を無効化(ボーダレスはF11で切り替える)
	factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
	factory->Release();

	m_hWnd = hWnd;
	m_realWidth = w;
	m_realHeight = h;

	// スワップチェインの実バックバッファ(ウィンドウのクライアントサイズ)を取得
	if (AcquireRealBackBuffer() == false) {
		errMsg = "バックバッファ取得失敗";

		Release();
		return false;
	}

	// 論理解像度(w x h)のオフスクリーンRTを作成し、既存コードからはこれをバックバッファとして扱う
	{
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = (UINT)w;
		desc.Height = (UINT)h;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		ID3D11Texture2D* pLogical = nullptr;
		bool ok = SUCCEEDED(m_pDevice->CreateTexture2D(&desc, nullptr, &pLogical))
			&& SUCCEEDED(m_pDevice->CreateShaderResourceView(pLogical, nullptr, &m_logicalSRV));

		if (ok)
		{
			m_backBuffer = std::make_shared<KdTexture>();
			ok = m_backBuffer->Create(pLogical);
		}
		KdSafeRelease(pLogical);

		if (ok == false)
		{
			errMsg = "バックバッファ作成失敗";
			Release();
			return false;
		}
	}

	// 拡大表示用のシェーダーとステートを作成
	if (CreateBlitResources(errMsg) == false)
	{
		Release();
		return false;
	}

	/*
	// ALT+Enterでフルスクリーン不可にする
	{
		IDXGIDevice* pDXGIDevice;
		m_pDevice->QueryInterface<IDXGIDevice>(&pDXGIDevice);

		IDXGIAdapter* pDXGIAdapter;
		pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&pDXGIAdapter);

		IDXGIFactory* pIDXGIFactory;
		pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void **)&pIDXGIFactory);

		pIDXGIFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

		pDXGIDevice->Release();
		pDXGIAdapter->Release();
		pIDXGIFactory->Release();
	}
	*/

	//=========================================================
	// Zバッファ作成
	//=========================================================
	{
		// バッファ作成
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		desc.Width = (UINT)w;
		desc.Height = (UINT)h;
		desc.CPUAccessFlags = 0;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		m_zBuffer = std::make_shared<KdTexture>();
		if (m_zBuffer->Create(desc) == false)
		{
			errMsg = "Zバッファ作成失敗";
			Release();
			return false;
		}
	}

	//=========================================================
	// バックバッファ、Zバッファを描画ターゲットとしてデバイスコンテキストへセットする
	//=========================================================
	{
		ID3D11RenderTargetView* rtvs[] = { m_backBuffer->WorkRTView() };
		m_pDeviceContext->OMSetRenderTargets(1, rtvs, m_zBuffer->WorkDSView());
	}

	//=========================================================
	// ビューポートの設定
	//=========================================================
	{
		// ビューポートの設定
		D3D11_VIEWPORT vp;
		vp.Width = (float)w;
		vp.Height = (float)h;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		m_pDeviceContext->RSSetViewports(1, &vp);
	}

	//=========================================================
	// 描画ステートの初期設定
	//=========================================================

	// ブレンドステートセット
	ID3D11BlendState* bs = CreateBlendState(KdBlendMode::Alpha);
	m_pDeviceContext->OMSetBlendState(bs, Math::Color(0, 0, 0, 0), 0xFFFFFFFF);
	bs->Release();

	// サンプラーステートセット
	ID3D11SamplerState* ss0 = CreateSamplerState(KdSamplerFilterMode::Anisotropic, 4, KdSamplerAddressingMode::Wrap, false);
	// 各シェーダへセット
	m_pDeviceContext->VSSetSamplers(0, 1, &ss0);
	m_pDeviceContext->PSSetSamplers(0, 1, &ss0);
	m_pDeviceContext->GSSetSamplers(0, 1, &ss0);
	m_pDeviceContext->CSSetSamplers(0, 1, &ss0);
	ss0->Release();

	ID3D11SamplerState* ss1 = CreateSamplerState(KdSamplerFilterMode::Anisotropic, 4, KdSamplerAddressingMode::Clamp, false);
	// 各シェーダへセット
	m_pDeviceContext->VSSetSamplers(1, 1, &ss1);
	m_pDeviceContext->PSSetSamplers(1, 1, &ss1);
	m_pDeviceContext->GSSetSamplers(1, 1, &ss1);
	m_pDeviceContext->CSSetSamplers(1, 1, &ss1);
	ss1->Release();

	// ラスタライザーステートセット
	ID3D11RasterizerState* rs = CreateRasterizerState(D3D11_CULL_BACK, D3D11_FILL_SOLID, true, false);
	m_pDeviceContext->RSSetState(rs);
	rs->Release();

	// 深度ステンシルステートセット
	ID3D11DepthStencilState* ds = CreateDepthStencilState(true, true);
	m_pDeviceContext->OMSetDepthStencilState(ds, 0);
	ds->Release();

	//=========================================================
	// 1x1の白テクスチャ作成
	//=========================================================
	{
		D3D11_SUBRESOURCE_DATA srd;
		auto whiteCol = kWhiteColor.RGBA();
		srd.pSysMem = &whiteCol;
		srd.SysMemPitch = 4;
		srd.SysMemSlicePitch = 0;

		m_texWhite = std::make_shared<KdTexture>();
		m_texWhite->Create(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1, &srd);
	}

	//=========================================================
	// 1x1のZ向き法線マップ作成
	//=========================================================
	{
		D3D11_SUBRESOURCE_DATA srd;
		auto normalCol = kNormalColor.RGBA();
		srd.pSysMem = &normalCol;
		srd.SysMemPitch = 4;
		srd.SysMemSlicePitch = 0;

		m_texNormal = std::make_shared<KdTexture>();
		m_texNormal->Create(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1, &srd);
	}

	// DrawVertices用頂点バッファを作成
	UINT bufferSize = 80;
	for (int i = 0; i < 10; i++)	// 
	{
		m_tempFixedVertexBuffer[i].Create(D3D11_BIND_VERTEX_BUFFER, bufferSize, D3D11_USAGE_DYNAMIC, nullptr);

		bufferSize *= 2;	// 容量を倍にしていく
	}

	m_windowWidth = w;
	m_windowHeight = h;
	m_isFullScreen = false;

	return true;
}

HRESULT KdDirect3D::SetFullscreenState(bool isFullscreen, IDXGIOutput* pTarget)
{
	m_isFullScreen = isFullscreen;
	return WorkSwapChain()->SetFullscreenState(m_isFullScreen, pTarget);
}

void KdDirect3D::Release()
{
	for (auto&& n : m_tempFixedVertexBuffer)
	{
		n.Release();
	}
	m_tempVertexBuffer.Release();

	m_realBackBuffer = nullptr;
	KdSafeRelease(m_logicalSRV);
	KdSafeRelease(m_blitVS);
	KdSafeRelease(m_blitPS);
	KdSafeRelease(m_blitSampler);
	KdSafeRelease(m_blitBlend);
	KdSafeRelease(m_blitDepth);
	KdSafeRelease(m_blitRaster);

	m_backBuffer = nullptr;
	m_zBuffer = nullptr;
	m_texWhite = nullptr;
	m_texNormal = nullptr;

	// スワップ チェーンをフルスクリーン モードで解放できないことがあるらしい。
	// フルスクリーンの場合ウィンドウモードに切り替えてから開放する必要があるらしい。
	if (m_isFullScreen)
	{
		if (m_pGISwapChain) m_pGISwapChain->SetFullscreenState(FALSE, 0);
	}

	KdSafeRelease(m_pGISwapChain);
	KdSafeRelease(m_pDeviceContext);
	KdSafeRelease(m_pDevice);
}

ID3D11DepthStencilState* KdDirect3D::CreateDepthStencilState(bool zEnable, bool zWriteEnable) const
{
	// Zバッファの動作設定
	D3D11_DEPTH_STENCIL_DESC desc = {};

	desc.DepthEnable = zEnable;							// 深度テストを使用する
	desc.DepthWriteMask = zWriteEnable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

	desc.StencilEnable = FALSE;
	desc.StencilReadMask = 0;
	desc.StencilWriteMask = 0;

	// ステートオブジェクトを作成
	ID3D11DepthStencilState* state = nullptr;
	if (FAILED(m_pDevice->CreateDepthStencilState(&desc, &state)))return nullptr;

	return state;
}

ID3D11RasterizerState* KdDirect3D::CreateRasterizerState(D3D11_CULL_MODE cullMode, D3D11_FILL_MODE fillMode, bool depthClipEnable, bool scissorEnable) const
{
	// ラスタライズの動作設定
	D3D11_RASTERIZER_DESC desc = {};
	desc.FillMode = fillMode;
	desc.CullMode = cullMode;
	desc.FrontCounterClockwise = FALSE;
	desc.DepthBias = 0;
	desc.DepthBiasClamp = 0;
	desc.SlopeScaledDepthBias = 0;
	desc.DepthClipEnable = depthClipEnable;
	desc.ScissorEnable = scissorEnable;
	desc.MultisampleEnable = FALSE;
	desc.AntialiasedLineEnable = FALSE;

	// ステートオブジェクト作成
	ID3D11RasterizerState* state = nullptr;
	if (FAILED(m_pDevice->CreateRasterizerState(&desc, &state)))return nullptr;

	return state;
}

ID3D11SamplerState* KdDirect3D::CreateSamplerState(KdSamplerFilterMode filterType, UINT maxAnisotropy, KdSamplerAddressingMode addressingMode, bool comparisonModel) const
{
	D3D11_SAMPLER_DESC desc = {};
	// フィルタ
	switch (filterType) {
		// Point
	case KdSamplerFilterMode::Point:
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		break;
		// Linear
	case KdSamplerFilterMode::Linear:
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		break;
		// Anisotropic
	case KdSamplerFilterMode::Anisotropic:
		desc.Filter = D3D11_FILTER_ANISOTROPIC;
		desc.MaxAnisotropy = maxAnisotropy;
		break;
	}

	// アドレッシングモード
	switch (addressingMode) {
		// Wrap
	case KdSamplerAddressingMode::Wrap:
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		break;
		// Clamp
	case KdSamplerAddressingMode::Clamp:
		desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		break;
	}

	// 比較モードサンプラ
	if (comparisonModel) {
		desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
		desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;	// とりあえず今回は線形フィルタ固定で
	}
	// 通常サンプラ
	else {
		desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	}

	// 
	desc.MipLODBias = 0;
	desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 0;
	desc.MinLOD = 0;
	desc.MaxLOD = D3D11_FLOAT32_MAX;

	// ステートオブジェクト作成
	ID3D11SamplerState* state = nullptr;
	if (FAILED(m_pDevice->CreateSamplerState(&desc, &state)))return nullptr;

	return state;
}

ID3D11BlendState* KdDirect3D::CreateBlendState(KdBlendMode mode) const
{
	D3D11_BLEND_DESC desc = {};
	// 有効
	desc.RenderTarget[0].BlendEnable = TRUE;
	// 書き込みマスク　R,G,B,A全て出力する場合はALL
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	desc.IndependentBlendEnable = FALSE;
	desc.AlphaToCoverageEnable = FALSE;

	// ブレンドなし
	if (mode == KdBlendMode::NoBlend)
	{
		// 色の合成方法
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
		// アルファの合成方法
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	}
	// 半透明ブレンド
	else if (mode == KdBlendMode::Alpha)
	{
		// 色の合成方法
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		// アルファの合成方法
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	}
	// 加算ブレンド
	else if (mode == KdBlendMode::Add)
	{
		// 色の合成方法
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		// アルファの合成方法
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	}
	// 半透明ブレンド＋MRTスロット1は上書き(ブレンド無効)
	//	※IndependentBlendEnable=FALSEのままだと、スロット0用のAlphaブレンド式が
	//	  スロット1(カラーグレード除外マスク等、シェーダーが直接書きたい値)にも
	//	  そのまま適用されてしまい、スロット1側のSrcAlphaが未定義な値になる結果、
	//	  意図した値が書き込まれなくなる。そのためスロット1だけ独立してBlendEnable=FALSE
	//	  (シェーダー出力をそのまま上書き)にする
	else if (mode == KdBlendMode::AlphaMasked)
	{
		desc.IndependentBlendEnable = TRUE;

		// スロット0：通常のAlphaブレンドと同じ
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;

		// スロット1：ブレンドせず、シェーダーが出力した値をそのまま書き込む
		desc.RenderTarget[1].BlendEnable = FALSE;
		desc.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	}
	// 乗算ブレンド
	else if (mode == KdBlendMode::Multiply)
	{
		// 色の合成方法: 最終カラー = (SrcColor * DestColor) + (DestColor * 0)
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;

		// アルファの合成方法: Srcを0、Destを1にして背景のアルファ値をそのまま維持する
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	}
	// 減算ブレンド
	else if (mode == KdBlendMode::Subtract)
	{
		// 色の合成方法: 最終カラー = (DestColor * 1) - (SrcColor * SrcAlpha)
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_REV_SUBTRACT; // DestからSrcを引く
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;

		// アルファの合成方法: 減算すると背景のアルファが消えるため、維持する
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	}
	// 乗算済みアルファ (Pre-multiplied Alpha)
	else if (mode == KdBlendMode::PreMultipliedAlpha)
	{
		// 色の合成方法: 最終カラー = (SrcColor * 1) + (DestColor * (1 - SrcAlpha))
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;

		// アルファの合成方法
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	}
	// ステートオブジェクト作成
	ID3D11BlendState* state = nullptr;
	if (FAILED(m_pDevice->CreateBlendState(&desc, &state)))return nullptr;

	return state;
}

void KdDirect3D::DrawVertices(D3D_PRIMITIVE_TOPOLOGY topology, int vertexCount, const void* pVertexStream, UINT stride)
{
	assert(m_pDeviceContext && "デバイスコンテキストが見つかりません");

	// プリミティブトポロジーをセット
	m_pDeviceContext->IASetPrimitiveTopology(topology);

	// 全頂点の総バイトサイズ
	UINT totalSize = vertexCount * stride;

	// 最適な固定長バッファを検索
	KdBuffer* buffer = nullptr;
	for (auto&& n : m_tempFixedVertexBuffer)
	{
		if (totalSize < n.GetBufferSize())
		{
			buffer = &n;
			break;
		}
	}
	// 可変長のバッファを使用
	if (buffer == nullptr)
	{
		buffer = &m_tempVertexBuffer;

		// 頂点バッファのサイズが小さいときは、リサイズのため再作成する
		if (m_tempVertexBuffer.GetBufferSize() < totalSize) {
			m_tempVertexBuffer.Create(D3D11_BIND_VERTEX_BUFFER, totalSize, D3D11_USAGE_DYNAMIC, nullptr);
		}
	}

	//============================================================
	//
	// 単純なDISCARDでの書き込み。
	//  DISCARDは、新たなバッファを内部で作成し、前回のバッファは使い終わると無効にするものです。
	//  つまり書き込む度に新規のバッファになる感じです。
	//  バッファのサイズが大きいと、その分新規のバッファを作成する時間がかかってしまいます。
	//  これを改善したい場合は、「DISCARDとNO_OVERWRITEの組み合わせ技」の方法で行うほうがよいです。
	//
	//============================================================

	// 全頂点をバッファに書き込み(DISCARD指定)
	buffer->WriteData(pVertexStream, totalSize);

	// 頂点バッファーをデバイスへセット
	{
		UINT offset = 0;
		m_pDeviceContext->IASetVertexBuffers(0, 1, buffer->GetAddress(), &stride, &offset);
	}

	// 描画
	m_pDeviceContext->Draw(vertexCount, 0);
}


void KdDirect3D::ClearBackBuffer()
{
	// バックバッファクリア
	m_pDeviceContext->ClearRenderTargetView(m_backBuffer->WorkRTView(), kRedColor);

	// Zバッファクリア
	m_pDeviceContext->ClearDepthStencilView(m_zBuffer->WorkDSView(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

// スワップチェーンのバッファ0を実バックバッファとして取得する
bool KdDirect3D::AcquireRealBackBuffer()
{
	ID3D11Texture2D* pBackBuffer = nullptr;
	if (FAILED(m_pGISwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer))) {
		return false;
	}

	m_realBackBuffer = std::make_shared<KdTexture>();
	bool ok = m_realBackBuffer->Create(pBackBuffer);
	pBackBuffer->Release();

	if (ok == false) m_realBackBuffer = nullptr;
	return ok;
}

// ウィンドウのクライアントサイズに合わせてスワップチェーンをリサイズする
void KdDirect3D::SyncSwapChainSize()
{
	RECT rc = {};
	if (GetClientRect(m_hWnd, &rc) == FALSE) return;

	int cw = rc.right - rc.left;
	int ch = rc.bottom - rc.top;

	// 最小化中(サイズ0)や変化なしのときは何もしない
	if (cw <= 0 || ch <= 0) return;
	if (cw == m_realWidth && ch == m_realHeight) return;

	// バックバッファの参照を全て手放してからリサイズする
	m_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	m_realBackBuffer = nullptr;
	m_pDeviceContext->Flush();

	if (SUCCEEDED(m_pGISwapChain->ResizeBuffers(0, cw, ch, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH)))
	{
		m_realWidth = cw;
		m_realHeight = ch;
	}

	AcquireRealBackBuffer();
}

// 論理バッファを実バックバッファへアスペクト比を保って収める拡大率とオフセットを求める
void KdDirect3D::CalcLetterbox(float& scale, float& offX, float& offY) const
{
	scale = (std::min)((float)m_realWidth / m_windowWidth, (float)m_realHeight / m_windowHeight);
	offX = (m_realWidth - m_windowWidth * scale) * 0.5f;
	offY = (m_realHeight - m_windowHeight * scale) * 0.5f;
}

POINT KdDirect3D::ClientToLogical(POINT clientPos) const
{
	if (m_realWidth <= 0 || m_realHeight <= 0) return clientPos;

	float scale, offX, offY;
	CalcLetterbox(scale, offX, offY);

	return {
		(LONG)((clientPos.x - offX) / scale),
		(LONG)((clientPos.y - offY) / scale)
	};
}

// 論理バッファ→実バックバッファ拡大用のシェーダーとステートを作成する
bool KdDirect3D::CreateBlitResources(std::string& errMsg)
{
	// 頂点バッファ不要のフルスクリーン三角形
	static const char kBlitHlsl[] = R"(
Texture2D g_tex : register(t0);
SamplerState g_smp : register(s0);

struct VSOut
{
	float4 pos : SV_Position;
	float2 uv : TEXCOORD0;
};

VSOut VSMain(uint id : SV_VertexID)
{
	VSOut o;
	float2 uv = float2((id << 1) & 2, id & 2);
	o.pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
	o.uv = uv;
	return o;
}

float4 PSMain(VSOut i) : SV_Target
{
	return float4(g_tex.Sample(g_smp, i.uv).rgb, 1);
}
)";

	auto compile = [&](const char* entry, const char* target) -> ID3DBlob*
	{
		ID3DBlob* blob = nullptr;
		ID3DBlob* err = nullptr;
		HRESULT hr = D3DCompile(kBlitHlsl, sizeof(kBlitHlsl) - 1, nullptr, nullptr, nullptr, entry, target, 0, 0, &blob, &err);
		if (FAILED(hr))
		{
			errMsg = "拡大表示シェーダーのコンパイル失敗";
			if (err) errMsg += std::string(": ") + (const char*)err->GetBufferPointer();
			if (blob) { blob->Release(); blob = nullptr; }
		}
		if (err) err->Release();
		return blob;
	};

	ID3DBlob* vsBlob = compile("VSMain", "vs_4_0");
	ID3DBlob* psBlob = compile("PSMain", "ps_4_0");

	bool ok = vsBlob && psBlob
		&& SUCCEEDED(m_pDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_blitVS))
		&& SUCCEEDED(m_pDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_blitPS));

	if (vsBlob) vsBlob->Release();
	if (psBlob) psBlob->Release();

	if (ok == false)
	{
		if (errMsg.empty()) errMsg = "拡大表示シェーダー作成失敗";
		return false;
	}

	m_blitSampler = CreateSamplerState(KdSamplerFilterMode::Linear, 1, KdSamplerAddressingMode::Clamp, false);
	m_blitBlend = CreateBlendState(KdBlendMode::NoBlend);
	m_blitDepth = CreateDepthStencilState(false, false);
	m_blitRaster = CreateRasterizerState(D3D11_CULL_NONE, D3D11_FILL_SOLID, true, false);

	if (!m_blitSampler || !m_blitBlend || !m_blitDepth || !m_blitRaster)
	{
		errMsg = "拡大表示ステート作成失敗";
		return false;
	}

	return true;
}

void KdDirect3D::BeginFinalPass(bool editorMode)
{
	ID3D11DeviceContext* ctx = m_pDeviceContext;
	SavedPipeline& s = m_saved;

	// 描画状態を退避
	ctx->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, s.rtv, &s.dsv);
	s.vpCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	ctx->RSGetViewports(&s.vpCount, s.vp);
	ctx->IAGetInputLayout(&s.il);
	ctx->IAGetPrimitiveTopology(&s.topo);
	ctx->VSGetShader(&s.vs, nullptr, nullptr);
	ctx->GSGetShader(&s.gs, nullptr, nullptr);
	ctx->PSGetShader(&s.ps, nullptr, nullptr);
	ctx->OMGetBlendState(&s.bs, s.bf, &s.mask);
	ctx->OMGetDepthStencilState(&s.ds, &s.ref);
	ctx->RSGetState(&s.rs);
	ctx->PSGetShaderResources(0, 1, &s.srv);
	ctx->PSGetSamplers(0, 1, &s.smp);

	// ウィンドウサイズの変化に実バックバッファを追従させる
	SyncSwapChainSize();
	if (m_realBackBuffer == nullptr) return;

	// 実バックバッファを全面ビューポートで描画先にする
	ID3D11RenderTargetView* rtv = m_realBackBuffer->WorkRTView();
	ctx->OMSetRenderTargets(1, &rtv, nullptr);

	D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)m_realWidth, (float)m_realHeight, 0.0f, 1.0f };
	ctx->RSSetViewports(1, &vp);

	// エディタ表示中は従来のクリア色、それ以外は黒帯用に黒でクリア
	if (editorMode)
	{
		ctx->ClearRenderTargetView(rtv, kRedColor);
	}
	else
	{
		const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		ctx->ClearRenderTargetView(rtv, black);
	}
}

void KdDirect3D::BlitLogicalToReal()
{
	if (m_realBackBuffer == nullptr || m_blitVS == nullptr) return;

	ID3D11DeviceContext* ctx = m_pDeviceContext;

	// アスペクト比を保つビューポート(余白は黒帯)
	float scale, offX, offY;
	CalcLetterbox(scale, offX, offY);
	D3D11_VIEWPORT vp = { offX, offY, m_windowWidth * scale, m_windowHeight * scale, 0.0f, 1.0f };
	ctx->RSSetViewports(1, &vp);

	ctx->IASetInputLayout(nullptr);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->VSSetShader(m_blitVS, nullptr, 0);
	ctx->GSSetShader(nullptr, nullptr, 0);
	ctx->PSSetShader(m_blitPS, nullptr, 0);
	ctx->PSSetShaderResources(0, 1, &m_logicalSRV);
	ctx->PSSetSamplers(0, 1, &m_blitSampler);
	ctx->OMSetBlendState(m_blitBlend, nullptr, 0xFFFFFFFF);
	ctx->OMSetDepthStencilState(m_blitDepth, 0);
	ctx->RSSetState(m_blitRaster);

	ctx->Draw(3, 0);

	// 論理バッファを次フレームで描画先にできるようSRVを外す
	ID3D11ShaderResourceView* nullSRV = nullptr;
	ctx->PSSetShaderResources(0, 1, &nullSRV);

	// 以降のUI描画用に全面ビューポートへ戻す
	D3D11_VIEWPORT full = { 0.0f, 0.0f, (float)m_realWidth, (float)m_realHeight, 0.0f, 1.0f };
	ctx->RSSetViewports(1, &full);
}

void KdDirect3D::EndFinalPass()
{
	ID3D11DeviceContext* ctx = m_pDeviceContext;
	SavedPipeline& s = m_saved;

	// 退避した描画状態を復元
	ctx->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, s.rtv, s.dsv);
	ctx->RSSetViewports(s.vpCount, s.vp);
	ctx->IASetInputLayout(s.il);
	ctx->IASetPrimitiveTopology(s.topo);
	ctx->VSSetShader(s.vs, nullptr, 0);
	ctx->GSSetShader(s.gs, nullptr, 0);
	ctx->PSSetShader(s.ps, nullptr, 0);
	ctx->OMSetBlendState(s.bs, s.bf, s.mask);
	ctx->OMSetDepthStencilState(s.ds, s.ref);
	ctx->RSSetState(s.rs);
	ctx->PSSetShaderResources(0, 1, &s.srv);
	ctx->PSSetSamplers(0, 1, &s.smp);

	// 取得時に増えた参照を解放
	for (auto& r : s.rtv) KdSafeRelease(r);
	KdSafeRelease(s.dsv);
	KdSafeRelease(s.il);
	KdSafeRelease(s.vs);
	KdSafeRelease(s.gs);
	KdSafeRelease(s.ps);
	KdSafeRelease(s.bs);
	KdSafeRelease(s.ds);
	KdSafeRelease(s.rs);
	KdSafeRelease(s.srv);
	KdSafeRelease(s.smp);
}
