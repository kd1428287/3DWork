#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

//============================================================
// アプリケーションクラス
//	APP.～ でどこからでもアクセス可能
//============================================================
class Application
{
	// メンバ
public:

	// アプリケーション実行
	void Execute();

	// アプリケーション終了
	void End() { m_endFlag = true; }

	// Updateスレッドのエントリ関数(専用スレッドで実行される)
	void UpdateThreadMain();

	HWND GetWindowHandle()		const { return m_window.GetWndHandle(); }
	int GetMouseWheelValue()	const { return m_window.GetMouseWheelVal(); }

	int		GetNowFPS()			const { return m_fpsController.m_nowfps; }
	int		GetMaxFPS()			const { return m_fpsController.m_maxFps; }
	float	GetDeltaTime()		const { return m_fpsController.GetDeltaTime(); }
private:

	void KdBeginUpdate();
	void Update();
	void KdPostUpdate();

	void KdBeginDraw(bool usePostProcess = true);
	void PreDraw();
	void Draw();
	void PostDraw();
	void DrawSprite();
	void KdPostDraw();

	// アプリケーション初期化
	bool Init(int w, int h);

	// アプリケーション解放
	void Release();

	// ゲームウィンドウクラス
	KdWindow		m_window;

	// FPSコントローラー
	KdFPSController	m_fpsController;

	// ゲーム終了フラグ trueで終了する (メインスレッドのみが書き込む)
	std::atomic<bool>	m_endFlag = false;

	//=====================================================
	// マルチスレッド関連
	//	Update処理とRender処理を別スレッドで実行する
	//	・メインスレッド … ウィンドウメッセージ処理 + 描画(Render)処理
	//	・Updateスレッド … ゲームの更新(Update)処理
	//	Render側がゲームオブジェクトの読み取りを終えた時点(DrawSprite直後)で
	//	次フレームのUpdateスレッドに手番を渡すことで、
	//	ImGui描画やPresent(Vsync待ち)とUpdate処理を並行実行できるようにしている。
	//=====================================================
	std::thread				m_updateThread;		// Update専用スレッド

	std::mutex				m_syncMutex;		// 手番切り替え用ミューテックス
	std::condition_variable	m_cvUpdate;			// Updateスレッド起床通知用
	std::condition_variable	m_cvRender;			// メイン(描画)スレッド起床通知用

	enum class FrameTurn { Update, Render };
	FrameTurn				m_turn = FrameTurn::Update;	// 現在どちらの手番か

	std::atomic<bool>		m_threadExit = false;	// Updateスレッドへの終了要求

	//=====================================================
	// シングルトンパターン
	//=====================================================
private:
	// 
	Application() {}

public:
	static Application& Instance() {
		static Application Instance;
		return Instance;
	}
};