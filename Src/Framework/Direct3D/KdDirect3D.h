#pragma once

class KdTexture;

// サンプラステート
enum class KdSamplerFilterMode {
	Point,			// ポイントフィルタ
	Linear,			// 線形フィルタ
	Anisotropic,	// 異方性フィルタ
};

enum class KdSamplerAddressingMode {
	Wrap,			// ラップモード
	Clamp,			// クランプモード
};

// ブレンドモード用定数
enum class KdBlendMode {
	NoBlend,		// ブレンドをしない
	Alpha,			// 半透明ブレンド
	Add,			// 加算ブレンド
	AlphaMasked,	// 半透明ブレンド＋MRTスロット1(カラーグレード除外マスク等)はブレンドせず上書き
	Multiply,			// 乗算	
	Subtract,		// 減算
	PreMultipliedAlpha,		// 乗算済みアルファ
};

//=======================================================================================
//
// Direct3Dクラス
//
//	Direct3D11に必要な変数や、よく行う操作をまとめています。
//
//=======================================================================================
class KdDirect3D {
public:

	//==============================================================
	//
	// 取得
	//
	//==============================================================

	// Direct3Dデバイス取得
	const ID3D11Device* GetDev() { return m_pDevice; }
	ID3D11Device* WorkDev() { return m_pDevice; }

	// Direct3Dデバイスコンテキスト取得
	const ID3D11DeviceContext* GetDevContext() { return m_pDeviceContext; }
	ID3D11DeviceContext* WorkDevContext() { return m_pDeviceContext; }

	// SwapChain取得
	const IDXGISwapChain* GetSwapChain() { return m_pGISwapChain; }
	IDXGISwapChain* WorkSwapChain() { return m_pGISwapChain; }

	// バックバッファ取得
	const std::shared_ptr<KdTexture> GetBackBuffer() { return m_backBuffer; }
	std::shared_ptr<KdTexture>		 WorkBackBuffer() { return m_backBuffer; }

	// Zバッファ取得
	const std::shared_ptr<KdTexture> GetZBuffer() { return m_zBuffer; }
	std::shared_ptr<KdTexture>		 WorkZBuffer() { return m_zBuffer; }

	// ビューポート取得
	void						CopyViewportInfo(Math::Viewport& out) const;

	// 1x1 白テクスチャ取得
	const std::shared_ptr<KdTexture>	GetWhiteTex() const { return m_texWhite; }

	// 1x1 Z向き法線マップ取得
	const std::shared_ptr<KdTexture>	GetNormalTex() const { return m_texNormal; }

	// スクリーン座標から3D座標への変換
	void ClientToWorld(const POINT& screenPos, float porjZ, Math::Vector3& dst, const Math::Matrix& mCam, const Math::Matrix& mProj);
	void WorldToClient(const Math::Vector3& srcWorld, POINT& dst, const Math::Matrix& mCam, const Math::Matrix& mProj);

	// 論理解像度(ゲームが描画する固定解像度)
	int GetLogicalWidth()  const { return m_windowWidth; }
	int GetLogicalHeight() const { return m_windowHeight; }

	// 実バックバッファの解像度(ウィンドウのクライアント領域サイズ)
	int GetRealWidth()  const { return m_realWidth; }
	int GetRealHeight() const { return m_realHeight; }

	// クライアント座標を論理座標へ変換(黒帯分のオフセットと拡大率を補正)
	POINT ClientToLogical(POINT clientPos) const;

	//==============================================================
	//
	// 設定
	//
	//==============================================================
	// フルスクリーン設定
	HRESULT SetFullscreenState(bool isFullscreen, IDXGIOutput* pTarget);

	//==============================================================
	//
	// 初期化・解放
	//
	//==============================================================

	// 初期化
	// ・hWnd			… ウィンドウのハンドル
	// ・w				… X解像度
	// ・h				… Y解像度
	// ・debugDevice	… デバッグモード
	// ・errMsg			… (出力)エラーメッセージが入ってくる
	bool Init(HWND hWnd, int w, int h, bool deviceDebug, std::string& errMsg);

	// 解放
	void Release();

	//==============================================================
	//
	// パイプラインステート作成関数
	//
	//==============================================================

	// 深度ステンシルステート作成
	// ・device				… D3Dデバイス
	// ・zEnable			… 深度テスト有効
	// ・zWriteEnable		… 深度書き込み有効
	ID3D11DepthStencilState* CreateDepthStencilState(bool zEnable, bool zWriteEnable) const;

	// ラスタライザーステート作成
	// ・device				… D3Dデバイス
	// ・cullMode			… カリングモード
	// ・fillMode			… 三角形の描画モード
	// ・depthClipEnable	… Zクリッピングを有効にする
	// ・scissorEnable		… 切り抜き範囲を有効にする
	ID3D11RasterizerState* CreateRasterizerState(D3D11_CULL_MODE cullMode, D3D11_FILL_MODE fillMode, bool depthClipEnable, bool scissorEnable) const;

	// サンプラーステート作成
	// ・device				… D3Dデバイス
	// ・filterType			… 0:補間なし 1:線形フィルタ 2:異方性フィルタ
	// ・maxAnisotropy		… 異方性フィルタ時の最大値　2, 4, 6, 8, 10, 12, 14, 16 のいずれか
	// ・addressingMode		… テクスチャアドレッシングモード 0:Wrap 1:Clamp
	// ・comparisonModel	… 比較モードON　シャドウマッピングなどで使用する
	ID3D11SamplerState* CreateSamplerState(KdSamplerFilterMode filterType, UINT maxAnisotropy, KdSamplerAddressingMode addressingMode, bool comparisonModel) const;

	// ブレンドステート作成
	// ・device				… D3Dデバイス
	// ・mode				… 合成モード
	ID3D11BlendState* CreateBlendState(KdBlendMode mode) const;

	//==============================================================

	// 頂点を描画する簡易的な関数
	// ・topology		… 頂点をどのような形状で描画するか　D3D_PRIMITIVE_TOPOLOGYマクロを使用
	// ・vertexCount	… 頂点数
	// ・pVertexStream	… 頂点配列の先頭アドレス
	// ・stride			… １頂点のバイトサイズ
	void DrawVertices(D3D_PRIMITIVE_TOPOLOGY topology, int vertexCount, const void* pVertexStream, UINT stride);

	//==============================================================

	void SetBackBufferColor(const Math::Color& col) { m_backBafferClearColor = col; }
	void ClearBackBuffer();

	//==============================================================
	// 最終出力パス
	//  Begin → (Blit) → UI描画など → End → Present の順に呼ぶ
	//==============================================================
	// 描画状態を退避し、実バックバッファを描画先にする
	void BeginFinalPass(bool editorMode);

	// 論理バッファをアスペクト比を保って実バックバッファへ拡大描画する
	void BlitLogicalToReal();

	// 退避した描画状態(論理バッファへの描画先など)を元に戻す
	void EndFinalPass();

private:

	//==============================================================
	// Direct3D11の中心になるクラス。
	// 全体の管理とバッファ、シェーダ、テクスチャなどのリソース作成などを行う。
	// D3D9とは違って、このクラスは描画関係のメンバ関数を持たない。
	//==============================================================
	ID3D11Device* m_pDevice = nullptr;

	//==============================================================
	// 描画処理を行うクラス。
	// 内部的には、レンダリングコマンドと呼ばれるバイナリデータを作成し、GPUに送る。
	// GPUがレンダリングコマンドを解析することで実際の描画処理が行われる。
	//==============================================================
	ID3D11DeviceContext* m_pDeviceContext = nullptr;

	//==============================================================
	// ウインドウへの表示やダブルバッファリングなどを行うクラス。
	// マルチサンプリング、リフレッシュレートなどの設定もできる。
	//==============================================================
	IDXGISwapChain* m_pGISwapChain = nullptr;


	// DrawVertices用 頂点バッファ
	KdBuffer		m_tempFixedVertexBuffer[10];// 固定長 頂点バッファ
	KdBuffer		m_tempVertexBuffer;			// 可変長 頂点バッファ

	int				m_windowWidth = 1280;
	int				m_windowHeight = 780;

	Math::Color		m_backBafferClearColor = { 0.9f,0.9f,0.9f,1.0f };

	// 最終出力パスで退避する描画状態
	struct SavedPipeline
	{
		ID3D11RenderTargetView*     rtv[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
		ID3D11DepthStencilView*     dsv = nullptr;
		D3D11_VIEWPORT              vp[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
		UINT                        vpCount = 0;
		ID3D11InputLayout*          il = nullptr;
		D3D_PRIMITIVE_TOPOLOGY      topo = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
		ID3D11VertexShader*         vs = nullptr;
		ID3D11GeometryShader*       gs = nullptr;
		ID3D11PixelShader*          ps = nullptr;
		ID3D11BlendState*           bs = nullptr;
		FLOAT                       bf[4] = {};
		UINT                        mask = 0;
		ID3D11DepthStencilState*    ds = nullptr;
		UINT                        ref = 0;
		ID3D11RasterizerState*      rs = nullptr;
		ID3D11ShaderResourceView*   srv = nullptr;
		ID3D11SamplerState*         smp = nullptr;
	};

	bool AcquireRealBackBuffer();
	void SyncSwapChainSize();
	void CalcLetterbox(float& scale, float& offX, float& offY) const;
	bool CreateBlitResources(std::string& errMsg);

	HWND                        m_hWnd = nullptr;           // スワップチェーンの出力先ウィンドウ
	int                         m_realWidth = 0;            // 実バックバッファの幅
	int                         m_realHeight = 0;           // 実バックバッファの高さ
	std::shared_ptr<KdTexture>  m_realBackBuffer = nullptr; // 実バックバッファ(スワップチェーン)

	// 拡大表示用リソース
	ID3D11ShaderResourceView*   m_logicalSRV = nullptr;
	ID3D11VertexShader*         m_blitVS = nullptr;
	ID3D11PixelShader*          m_blitPS = nullptr;
	ID3D11SamplerState*         m_blitSampler = nullptr;
	ID3D11BlendState*           m_blitBlend = nullptr;
	ID3D11DepthStencilState*    m_blitDepth = nullptr;
	ID3D11RasterizerState*      m_blitRaster = nullptr;

	SavedPipeline               m_saved;

	// バッファ
	std::shared_ptr<KdTexture>	m_backBuffer = nullptr;		// 論理解像度のオフスクリーンRT(既存コードはこれをバックバッファとして扱う)
	std::shared_ptr<KdTexture>	m_zBuffer = nullptr;		// Zバッファ

	//------------------------
	// 便利テクスチャ
	//------------------------
	// 1x1 白テクスチャ
	std::shared_ptr<KdTexture>	m_texWhite = nullptr;

	// デフォルト法線マップ（1x1 Z向き）
	std::shared_ptr<KdTexture>	m_texNormal = nullptr;

	bool						m_isFullScreen = false;		// フルスクリーン動作かどうか

	//-------------------------------
	// シングルトン
	//-------------------------------
private:

	KdDirect3D() {}

	~KdDirect3D()
	{
		Release();
	}

public:
	static KdDirect3D& Instance() {
		static KdDirect3D instance;
		return instance;
	}
};