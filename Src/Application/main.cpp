#include "main.h"

#include "Core/Scene/SceneManager.h"
#include "Core/Scene/TransitionController.h"
#include "Systems/SelectableManager.h"

#include "Editor/EditorHost.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エントリーポイント
// アプリケーションはこの関数から進行する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_  HINSTANCE, _In_ LPSTR, _In_ int)
{
	// メモリリークを知らせる
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	// COM初期化
	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
	{
		CoUninitialize();

		return 0;
	}

	// mbstowcs_s関数で日本語対応にするために呼ぶ
	setlocale(LC_ALL, "japanese");

	//===================================================================
	// 実行]
	//===================================================================
	Application::Instance().Execute();

	// COM解放
	CoUninitialize();

	return 0;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新開始
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdBeginUpdate()
{
	// 入力状況の更新
	KdInputManager::Instance().Update();

	// UIのホバー・決定・キー移動(入力更新の直後、シーン更新の前に1回だけ呼ぶ)
	// 画面遷移中は操作を受け付けない
	if (!TransitionController::Instance().IsActive())
	{
		SelectableManager::Instance().Update();
	}

	// 空間環境の更新
	KdShaderManager::Instance().WorkAmbientController().Update();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新終了
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdPostUpdate()
{
	// 3DSoundListnerの行列を更新
	m_audioManager->SetListenerMatrix(KdShaderManager::Instance().GetCameraCB().mView.Invert());
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::Update()
{
	// SceneManager::Update()より前に回す。Covered通知で予約したシーンが同フレーム先頭で切り替わる
	TransitionController::Instance().Update(GetDeltaTime());

	SceneManager::Instance().Update();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画開始
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdBeginDraw(bool usePostProcess)
{
	// 3D描画先を切り替える
	//	・エディタ表示中 … オフスクリーン(Sceneウィンドウ用バッファ)
	//	・エディタ非表示中 … バックバッファへ直接フルスクリーン描画
	EditorHost::Instance().BeginSceneDraw();

	// エディタ非表示中は、描画先とビューポートを論理バッファ全面(1280x720)に固定する
	if (!EditorHost::Instance().IsEnabled())
	{
		auto& d3d = KdDirect3D::Instance();

		ID3D11RenderTargetView* rtvs[] = { d3d.WorkBackBuffer()->WorkRTView() };
		d3d.WorkDevContext()->OMSetRenderTargets(1, rtvs, d3d.WorkZBuffer()->WorkDSView());

		D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)d3d.GetLogicalWidth(), (float)d3d.GetLogicalHeight(), 0.0f, 1.0f };
		d3d.WorkDevContext()->RSSetViewports(1, &vp);
	}

	KdShaderManager::Instance().WorkAmbientController().Draw();

	if (!usePostProcess) return;
	KdShaderManager::Instance().m_postProcessShader.Draw();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画終了
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdPostDraw()
{
	auto& d3d = KdDirect3D::Instance();

	// エディタ表示中は従来通り論理バッファ(Zバッファ含む)をクリアしておく
	const bool editorMode = EditorHost::Instance().IsEnabled();
	if (editorMode)
	{
		d3d.ClearBackBuffer();
	}

	// 以降は実バックバッファ(ウィンドウ実サイズ)への最終出力
	d3d.BeginFinalPass(editorMode);

	// ゲーム画面(1280x720)をアスペクト比を保って拡大(エディタ表示中はUI側が表示する)
	if (!editorMode)
	{
		d3d.BlitLogicalToReal();
	}

	// エディタ用ドッキングUI一式の描画(非表示中は軽量な空処理)
	EditorHost::Instance().Draw();

	d3d.EndFinalPass();

	// BackBuffer -> 画面表示
	d3d.WorkSwapChain()->Present(0, 0);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画の前処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::PreDraw()
{
	SceneManager::Instance().PreDraw();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::Draw()
{
	SceneManager::Instance().Draw();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画の後処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::PostDraw()
{
	KdShaderManager::Instance().m_postProcessShader.UpdateDistortion(GetDeltaTime());

	// 画面のぼかしや被写界深度処理の実施
	KdShaderManager::Instance().m_postProcessShader.PostEffectProcess();

	// 現在のシーンのデバッグ描画
	SceneManager::Instance().DrawDebug();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 2Dスプライトの描画
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::DrawSprite()
{
	SceneManager::Instance().DrawSprite();

	// UIを含む画面全体を覆う(遷移中のみ)
	KdShaderManager::Instance().m_postProcessShader.DrawTransitionCover();
}

// 枠付きウィンドウ⇔モニター全体のボーダレスを切り替える
void Application::ToggleBorderless()
{
	HWND hwnd = m_window.GetWndHandle();

	if (!m_borderless)
	{
		m_prevStyle = GetWindowLongPtr(hwnd, GWL_STYLE);
		GetWindowPlacement(hwnd, &m_prevPlacement);

		MONITORINFO mi = { sizeof(mi) };
		GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
		m_clipRect = mi.rcMonitor;

		SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
		SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
			mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
	}
	else
	{
		ClipCursor(nullptr);

		SetWindowLongPtr(hwnd, GWL_STYLE, m_prevStyle);
		SetWindowPlacement(hwnd, &m_prevPlacement);
		SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}

	m_borderless = !m_borderless;
}

bool Application::IsAppForeground() const
{
	HWND fg = GetForegroundWindow();
	if (!fg) { return false; }

	DWORD pid = 0;
	GetWindowThreadProcessId(fg, &pid);
	return pid == GetCurrentProcessId();
}

// クライアント座標を設計解像度の座標へ変換する(黒帯と拡大率を補正)
POINT Application::ClientToVirtual(POINT clientPos) const
{
	return KdDirect3D::Instance().ClientToLogical(clientPos);
}

// 現在のマウス位置を設計解像度の座標で取得する
POINT Application::GetVirtualMousePos() const
{
	POINT p;
	GetCursorPos(&p);
	ScreenToClient(m_window.GetWndHandle(), &p);
	return ClientToVirtual(p);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 起動モード選択
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
Application::AppMode Application::SelectStartupMode()
{
#ifdef EDITOR_ENABLED
	// 日本語はワイド文字列で渡す(MessageBoxAはソース文字コード設定次第で文字化けする)
	int result = MessageBoxW(m_window.GetWndHandle(),
		L"エディタのみ起動しますか？\n\n[はい] エディタのみ\n[いいえ] ゲームをプレイ",
		L"起動モード選択", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

	return (result == IDYES) ? AppMode::EditorOnly : AppMode::Play;
#else
	return AppMode::Play;
#endif
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション初期設定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool Application::Init(int w, int h)
{
	//===================================================================
	// ウィンドウ作成
	//===================================================================
	if (m_window.Create(w, h, "幽玄調伏伝", "Window") == false) {
		MessageBoxA(nullptr, "ウィンドウ作成に失敗", "エラー", MB_OK);
		return false;
	}

	//===================================================================
	// フルスクリーン確認
	//===================================================================
	bool bFullScreen = false;
	//	if (MessageBoxA(m_window.GetWndHandle(), "フルスクリーンにしますか？", "確認", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES) {
	//		bFullScreen = true;
	//	}

	//===================================================================
	// Direct3D初期化
	//===================================================================

	// デバイスのデバッグモードを有効にする
	bool deviceDebugMode = false;
#ifdef _DEBUG
	deviceDebugMode = true;
#endif

	// Direct3D初期化
	std::string errorMsg;
	if (KdDirect3D::Instance().Init(m_window.GetWndHandle(), w, h, deviceDebugMode, errorMsg) == false) {
		MessageBoxA(m_window.GetWndHandle(), errorMsg.c_str(), "Direct3D初期化失敗", MB_OK | MB_ICONSTOP);
		return false;
	}

	// フルスクリーン設定
	if (bFullScreen) {
		HRESULT hr;

		hr = KdDirect3D::Instance().SetFullscreenState(TRUE, 0);
		if (FAILED(hr))
		{
			MessageBoxA(m_window.GetWndHandle(), "フルスクリーン設定失敗", "Direct3D初期化失敗", MB_OK | MB_ICONSTOP);
			return false;
		}
	}

	//===================================================================
	// エディタ初期化(imgui含む。KD_EDITOR_ENABLED未定義ビルドでは何もしない)
	//===================================================================
	EditorHost::Instance().Init(w, h);

	//===================================================================
	// シェーダー初期化
	//===================================================================
	KdShaderManager::Instance().Init();

	// 画面遷移の要求購読を、最初のシーン生成より前に済ませておく
	TransitionController::Instance();

	//===================================================================
	// オーディオ初期化
	//===================================================================
	m_audioManager = std::make_unique<KdAudioManager>();
	m_audioManager->Init();

	//===================================================================
	// フォント初期化
	//===================================================================
	KdFontManager::Instance().Init(GetWindowHandle());

	KdFontManager::Instance().AddFontResource(L"Asset/Data/Font/衡山毛筆フォント.ttf");
	KdFontManager::Instance().AddFont(0, L"衡山毛筆フォント", 32);

	//===================================================================
	// ゲーム固有の初期化
	//===================================================================
	ShowCursolEnable(true);

	// Input
	auto keyboardDevice = std::make_unique<KdInputCollector>();

	// ボタンの登録
	keyboardDevice->AddButton("Evade", new KdInputButtonForWindows(VK_SPACE));
	keyboardDevice->AddButton("Attack", new KdInputButtonForWindows({ 'Z', VK_LBUTTON }));
	keyboardDevice->AddButton("Guard", new KdInputButtonForWindows({ VK_RBUTTON }));
	keyboardDevice->AddButton("Dash", new KdInputButtonForWindows({ VK_LSHIFT }));
	keyboardDevice->AddButton("Pause", new KdInputButtonForWindows({ 'T' }));
	keyboardDevice->AddButton("Editor", new KdInputButtonForWindows({ VK_F1 }));
	keyboardDevice->AddButton("Lock", new KdInputButtonForWindows({ VK_MBUTTON }));

	// UI操作用(SelectableManagerが参照する。名前を変える場合はSetButtonNamesも合わせる)
	keyboardDevice->AddButton("UIConfirm", new KdInputButtonForWindows({ VK_LBUTTON, VK_RETURN, 'Z' }));
	keyboardDevice->AddButton("UIUp", new KdInputButtonForWindows({ 'W', VK_UP }));
	keyboardDevice->AddButton("UIDown", new KdInputButtonForWindows({ 'S', VK_DOWN }));
	keyboardDevice->AddButton("UILeft", new KdInputButtonForWindows({ 'A', VK_LEFT }));
	keyboardDevice->AddButton("UIRight", new KdInputButtonForWindows({ 'D', VK_RIGHT }));

	// Debug用
	keyboardDevice->AddButton("Debug2", new KdInputButtonForWindows({ VK_F2 }));
	keyboardDevice->AddButton("Debug3", new KdInputButtonForWindows({ VK_F3 }));
	keyboardDevice->AddButton("Debug4", new KdInputButtonForWindows({ VK_F4 }));
	keyboardDevice->AddButton("Debug5", new KdInputButtonForWindows({ VK_F5 }));
	keyboardDevice->AddButton("Debug6", new KdInputButtonForWindows({ VK_F6 }));
	keyboardDevice->AddButton("Debug7", new KdInputButtonForWindows({ VK_F7 }));

	std::string buff;
	for (int i = 0; i < 10; i++)
	{
		buff = std::to_string(i) + "key";
		keyboardDevice->AddButton(buff, new KdInputButtonForWindows('0' + i));
	}

	// 軸（2Dベクトル）の登録: "Move" アクションに [W, D, S, A] を割り当て
	// 引数の順序: 上(Up), 右(Right), 下(Down), 左(Left)
	auto up = std::make_shared<KdInputButtonForWindows>(std::vector<int>{ 'W', VK_UP });
	auto right = std::make_shared<KdInputButtonForWindows>(std::vector<int>{ 'D', VK_RIGHT });
	auto down = std::make_shared<KdInputButtonForWindows>(std::vector<int>{ 'S', VK_DOWN });
	auto left = std::make_shared<KdInputButtonForWindows>(std::vector<int>{ 'A', VK_LEFT });

	auto cross = std::make_shared<KdInputAxisForWindows>(
		up, right, down, left
	);
	keyboardDevice->AddAxis("Move", cross);

	// 軸の登録: "Look" アクションにマウスの移動量を割り当て
	auto lookAxis = std::make_shared<KdInputAxisForWindowsMouse>();
	keyboardDevice->AddAxis("Look", lookAxis);
	//lookAxis->SetConfineToWindowCenter(true);

	auto controllerDevice = std::make_unique<KdInputCollector>();

	//controllerDevice->AddAxis("Move",)

	// 3. マネージャーにデバイスを登録
	// ※内部で unique_ptr に変換されて管理されます
	KdInputManager::Instance().AddDevice("Keyboard", keyboardDevice);

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション実行
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::Execute()
{
	KdCSVData windowData("Asset/Data/WindowSettings.csv");
	const std::vector<std::string>& sizeData = windowData.GetLine(0);

	//===================================================================
	// 初期設定(ウィンドウ作成、Direct3D初期化など)
	//===================================================================
	if (Application::Instance().Init(atoi(sizeData[0].c_str()), atoi(sizeData[1].c_str())) == false) {
		return;
	}

	//===================================================================
	// 起動モード決定(EditorOnlyではエディタを常時表示にする)
	//===================================================================
	m_appMode = SelectStartupMode();

	if (m_appMode == AppMode::EditorOnly && !EditorHost::Instance().IsEnabled())
	{
		EditorHost::Instance().ToggleEnabled();
	}

	//===================================================================
	// ゲームループ
	//===================================================================

	// 時間
	m_fpsController.Init();

	// ループ
	while (1)
	{
		// 処理開始時間Get
		m_fpsController.UpdateStartTime();
		//KdDebugGUI::Instance().ClearLog();

		std::wstring str = L"幽玄調伏伝 FPS: " + std::to_wstring(Application::Instance().GetNowFPS());
		SetWindowTextW(m_window.GetWndHandle(), str.c_str());



		// ゲーム終了指定があるときはループ終了
		if (m_endFlag)
		{
			break;
		}

		//=========================================
		//
		// ウィンドウ関係の処理
		//
		//=========================================

		// ウィンドウのメッセージを処理する
		m_window.ProcessMessage();

		// ウィンドウが破棄されてるならループ終了
		if (m_window.IsCreated() == false)
		{
			break;
		}

		{
			bool escapeKeyDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;

			if (escapeKeyDown && !m_escapeKeyDown && IsAppForeground())
			{
				End();
			}

			m_escapeKeyDown = escapeKeyDown;
		}

		// エディタの表示更新ON/OFF(F1キー。押した瞬間のみトグルするためエッジ検出する)
		// EditorOnlyでは常時表示のためトグルしない
		if (m_appMode == AppMode::Play)
		{
			bool editorToggleKeyDown = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;

			if (editorToggleKeyDown && !m_prevEditorToggleKeyDown && IsAppForeground())
			{
				EditorHost::Instance().ToggleEnabled();
			}

			m_prevEditorToggleKeyDown = editorToggleKeyDown;
		}

		// ボーダレス切り替え(F11キー。押した瞬間のみ、ウィンドウが前面のときだけ)
		{
			bool borderlessKeyDown = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;

			if (borderlessKeyDown && !m_prevBorderlessKeyDown &&
				GetForegroundWindow() == m_window.GetWndHandle())
			{
				ToggleBorderless();
			}

			m_prevBorderlessKeyDown = borderlessKeyDown;
		}

		// ボーダレス中は、前面にいる間だけカーソルをモニター内に拘束する
		if (m_borderless)
		{
			ClipCursor(GetForegroundWindow() == m_window.GetWndHandle() ? &m_clipRect : nullptr);
		}

		//=========================================
		//
		// アプリケーション更新処理
		//
		//=========================================

		KdBeginUpdate();
		{
			// EditorOnlyではゲームの更新を行わない
			if (m_appMode == AppMode::Play)
			{
				Update();
			}
		}
		KdPostUpdate();

		//=========================================
		//
		// アプリケーション描画処理
		//
		//=========================================

		KdBeginDraw();
		{
			// EditorOnlyではゲームの描画を行わない
			if (m_appMode == AppMode::Play)
			{
				PreDraw();

				Draw();

				PostDraw();

				DrawSprite();
			}

			// エディタのプレビュー用ビューポートへの描画(エフェクト/マップ)
			// ※通常描画パイプライン(Draw〜PostDraw〜DrawSprite)が完全に終わった後に実施する。
			//   ここまででレンダーターゲット/パイプラインステートは全て元の状態に戻っているため、
			//   エディタ側の描画はこのクリーンな状態から独立して行える。
			//   KD_EDITOR_ENABLED未定義ビルドでは何もしない
			EditorHost::Instance().RenderPreviewViewports();
		}
		KdPostDraw();

		m_audioManager->Update();


		//=========================================
		//
		// フレームレート制御
		//
		//=========================================

		m_fpsController.Update();
	}
}

// アプリケーション終了
void Application::Release()
{
	// カーソル拘束を解除する
	ClipCursor(nullptr);

	// エディタ関連の解放(内部でImGuiのContext破棄まで行う)
	// KdDirect3D::Instance().Release()より前に呼ぶこと(ImGuiのDX11バックエンド解放が
	// デバイス/コンテキスト破棄後になると不正になるため)
	EditorHost::Instance().Release();

	KdInputManager::Instance().Release();

	// 画面遷移の購読解除(KdShaderManagerの解放より前に行う)
	TransitionController::Instance().Shutdown();

	KdShaderManager::Instance().Release();

	m_audioManager->Release();

	KdDirect3D::Instance().Release();

	// ウィンドウ削除
	m_window.Release();
}