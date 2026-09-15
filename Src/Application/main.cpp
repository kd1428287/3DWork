#include "main.h"

#include "Core/Scene/SceneManager.h"

#include "Editor/Common/EditorViewport.h"
#include "Editor/Tools/EffectEditor.h"
#include "Editor/Tools/MapEditor.h"

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

	// ※空間環境(アンビエント)の更新はDirect3Dの定数バッファ書き込みを伴うため、
	//   GPUに触れる処理としてKdBeginDraw()側(描画スレッド)に移動した
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新終了
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdPostUpdate()
{
	// 3DSoundListnerの行列を更新
	KdAudioManager::Instance().SetListnerMatrix(KdShaderManager::Instance().GetCameraCB().mView.Invert());
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::Update()
{
	// エディタのみモードでは、ゲームプレイ自体の更新は行わない
	// (プレビュー用に一部のオブジェクトだけ動かしたい場合はここに個別処理を追加してください)
	if (m_appMode == AppMode::EditorOnly)
	{
		return;
	}

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
	EditorViewport::Instance().BeginSceneDraw();

	// 空間環境(アンビエント)の更新・描画
	//	定数バッファへの書き込み(GPUアクセス)を伴うため、Updateスレッドではなくこちらで実行する
	KdShaderManager::Instance().WorkAmbientController().Update();
	KdShaderManager::Instance().WorkAmbientController().Draw();

	if (!usePostProcess) return;
	KdShaderManager::Instance().m_postProcessShader.Draw();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画終了
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdPostDraw()
{
	if (EditorViewport::Instance().IsEnabled())
	{
		// バックバッファをクリアし、ImGui(ドッキングUI)用のレンダーターゲットに戻す
		KdDirect3D::Instance().ClearBackBuffer();

		ID3D11RenderTargetView* rtvs[] = { KdDirect3D::Instance().WorkBackBuffer()->WorkRTView() };
		KdDirect3D::Instance().WorkDevContext()->OMSetRenderTargets(1, rtvs, KdDirect3D::Instance().WorkZBuffer()->WorkDSView());


	}
	// エディタ非表示中：ゲーム画面はBeginSceneDraw()で既にバックバッファへ直接描画済みのため、
	// ここで再クリアするとゲーム画面が消えてしまうので何もしない

	// Imguiのレンダリング(エディタ非表示中は中身が空でも軽量に呼べる)
	KdDebugGUI::Instance().GuiProcess();

	// BackBuffer -> 画面表示
	KdDirect3D::Instance().WorkSwapChain()->Present(0, 0);
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
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 起動モード選択
//	ウィンドウ/Direct3D初期化より前に呼び出す想定(ネイティブダイアログのみ使用)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
Application::AppMode Application::SelectStartupMode()
{
	// ANSI(MessageBoxA)は実行環境のコードページ次第で日本語が文字化けすることがあるため、
	// ソースのUTF-8をそのままUTF-16として扱えるワイド文字版(MessageBoxW)を使用する
	int result = MessageBoxW(
		nullptr,
		L"「はい」でエディタのみ起動します。\n「いいえ」でゲームをプレイします。",
		L"起動モード選択",
		MB_YESNO | MB_ICONQUESTION);

	m_appMode = (result == IDYES) ? AppMode::EditorOnly : AppMode::Play;

	return m_appMode;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション初期設定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool Application::Init(int w, int h)
{
	//===================================================================
	// ウィンドウ作成
	//===================================================================
	if (m_window.Create(w, h, "3D GameProgramming", "Window") == false) {
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
	// imgui初期化
	//===================================================================
	KdDebugGUI::Instance().GuiInit(w, h);

	//===================================================================
	// シェーダー初期化
	//===================================================================
	KdShaderManager::Instance().Init();

	//===================================================================
	// オーディオ初期化
	//===================================================================
	KdAudioManager::Instance().Init();

	//===================================================================
	// フォント初期化
	//===================================================================
	//KdFontManager::Instance().Init(GetWindowHandle());

	//===================================================================
	// ゲーム固有の初期化
	//===================================================================
	// カーソルを消す(エディタ表示中は"Pause"入力でShowCursor(TRUE)に切り替わる)
	ShowCursor(false);

	// Input
	// 1. キーボード用のコレクターを作成
	auto keyboardDevice = std::make_unique<KdInputCollector>();

	// ボタンの登録: "Jump" アクションに [スペースキー] を割り当て
	keyboardDevice->AddButton("Evade", new KdInputButtonForWindows(VK_SPACE));
	keyboardDevice->AddButton("Attack", new KdInputButtonForWindows({ 'Z', VK_LBUTTON }));
	keyboardDevice->AddButton("Guard", new KdInputButtonForWindows({ VK_RBUTTON }));
	keyboardDevice->AddButton("Dash", new KdInputButtonForWindows({ VK_LSHIFT }));
	keyboardDevice->AddButton("Pause", new KdInputButtonForWindows({ 'T' }));
	keyboardDevice->AddButton("Editor", new KdInputButtonForWindows({ VK_F1 }));
	keyboardDevice->AddButton("Lock", new KdInputButtonForWindows({ VK_MBUTTON }));

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
	lookAxis->SetConfineToWindowCenter(true);

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
	//===================================================================
	// 起動モード選択(エディタのみ / プレイ)
	//	ウィンドウ・Direct3D初期化より前に選ばせる
	//===================================================================
	SelectStartupMode();

	KdCSVData windowData("Asset/Data/WindowSettings.csv");
	const std::vector<std::string>& sizeData = windowData.GetLine(0);

	//===================================================================
	// 初期設定(ウィンドウ作成、Direct3D初期化など)
	//===================================================================
	if (Application::Instance().Init(atoi(sizeData[0].c_str()), atoi(sizeData[1].c_str())) == false) {
		return;
	}

	// エディタのみモードの場合は、起動直後からエディタ画面を開いた状態にする
	// ※EditorViewportに有効/無効を外部から切り替えるAPI(例:SetEnabled)が無い場合は追加してください
	if (m_appMode == AppMode::EditorOnly)
	{
		EditorViewport::Instance().SetEnabled(true);
	}

	//===================================================================
	// ゲームループ
	//===================================================================

	// 時間
	m_fpsController.Init();

	//===================================================================
	// Update/Renderスレッドの手番制御を初期化し、Updateスレッドを起動
	//	・Updateスレッド … ゲームの更新(Update)処理
	//	・このスレッド(メイン) … ウィンドウメッセージ処理 + 描画(Render)処理
	//===================================================================
	m_threadExit = false;
	m_turn = FrameTurn::Update;			// まずUpdateスレッドに1フレーム目を計算させる
	m_updateThread = std::thread(&Application::UpdateThreadMain, this);

	// ループ
	while (1)
	{
		// 処理開始時間Get
		m_fpsController.UpdateStartTime();
		//KdDebugGUI::Instance().ClearLog();

		std::string str = "3D_Action FPS: " + std::to_string(Application::Instance().GetNowFPS());
		SetWindowTextA(m_window.GetWndHandle(), str.c_str());



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

		if (GetAsyncKeyState(VK_ESCAPE))
		{
			//			if (MessageBoxA(m_window.GetWndHandle(), "本当にゲームを終了しますか？",
			//				"終了確認", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
			{
				End();
			}
		}

		//=========================================
		//
		// このスレッド(描画側)の手番が来るまで待機
		//	Updateスレッドが前フレームの更新を終えるまでここでブロックする
		//
		//=========================================

		OutputDebugStringA("[Render] cvRender待機開始\n");
		{
			std::unique_lock<std::mutex> lock(m_syncMutex);
			m_cvRender.wait(lock, [this] { return m_turn == FrameTurn::Render; });
		}
		OutputDebugStringA("[Render] cvRender待機終了 → 描画開始\n");

		//=========================================
		//
		// アプリケーション描画処理
		//
		//=========================================

		KdBeginDraw();
		{
			PreDraw();

			Draw();

			// エフェクトプレビュー専用ビューポートへの描画
			EffectEditor::Instance().RenderPreviewViewport();
			// マッププレビュー
			MapEditor::Instance().RenderPreviewViewport();


			PostDraw();

			DrawSprite();


		}

		//=========================================
		//
		// ゲームオブジェクトの読み取りが完了したので、
		// 次フレームのUpdateスレッドに手番を渡す
		// (この後のImGui描画・Present(Vsync待ち)はUpdateスレッドと並行実行される)
		//
		//=========================================

		OutputDebugStringA("[Render] 描画完了 → Updateスレッドへ手番を渡す\n");
		{
			std::lock_guard<std::mutex> lock(m_syncMutex);
			m_turn = FrameTurn::Update;
		}
		m_cvUpdate.notify_one();

		KdPostDraw();
		OutputDebugStringA("[Render] KdPostDraw完了(Present済み)\n");

		//=========================================
		//
		// フレームレート制御
		//
		//=========================================

		m_fpsController.Update();
	}

	//===================================================================
	// Updateスレッドの終了待ち
	//===================================================================
	m_threadExit = true;
	m_cvUpdate.notify_all();
	if (m_updateThread.joinable())
	{
		m_updateThread.join();
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Updateスレッドのエントリ関数
//	メインスレッドと手番(m_turn)を交互に受け渡しながらゲーム更新のみを行う
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::UpdateThreadMain()
{
	while (true)
	{
		// 自分の手番(Update)が来るまで待機
		OutputDebugStringA("[Update] cvUpdate待機開始\n");
		{
			std::unique_lock<std::mutex> lock(m_syncMutex);
			m_cvUpdate.wait(lock, [this] { return m_turn == FrameTurn::Update || m_threadExit; });

			if (m_threadExit)
			{
				break;
			}
		}
		OutputDebugStringA("[Update] cvUpdate待機終了 → 更新開始\n");

		//=========================================
		// アプリケーション更新処理
		//=========================================

		KdBeginUpdate();
		OutputDebugStringA("[Update] KdBeginUpdate完了\n");
		{
			Update();
		}
		OutputDebugStringA("[Update] Update完了\n");
		KdPostUpdate();
		OutputDebugStringA("[Update] KdPostUpdate完了 → 描画スレッドへ手番を渡す\n");

		// 更新完了。描画スレッドに手番を渡す
		{
			std::lock_guard<std::mutex> lock(m_syncMutex);
			m_turn = FrameTurn::Render;
		}
		m_cvRender.notify_one();
	}
}

// アプリケーション終了
void Application::Release()
{
	KdInputManager::Instance().Release();

	KdShaderManager::Instance().Release();

	KdAudioManager::Instance().Release();

	KdDirect3D::Instance().Release();

	// ウィンドウ削除
	m_window.Release();
}