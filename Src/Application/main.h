#pragma once

#include <atomic>

#include "FrameWork/Audio/KdAudio.h"

//============================================================
// アプリケーションクラス
//	APP.～ でどこからでもアクセス可能
//============================================================
class Application
{
	// メンバ
public:

	// 起動モード
	//	EditorOnly … エディタツールのみを起動する(ゲームプレイ更新は行わない)
	//	Play       … 通常通りゲームをプレイする
	enum class AppMode
	{
		Undecided,
		EditorOnly,
		Play,
	};

	// アプリケーション実行
	void Execute();

	// アプリケーション終了
	void End() { m_endFlag = true; }

	HWND GetWindowHandle()		const { return m_window.GetWndHandle(); }
	int GetMouseWheelValue()	const { return m_window.GetMouseWheelVal(); }

	int		GetNowFPS()			const { return m_fpsController.m_nowfps; }
	int		GetMaxFPS()			const { return m_fpsController.m_maxFps; }
	float	GetDeltaTime()		const { return m_fpsController.GetDeltaTime(); }

	void ShowCursolEnable(bool enable) {
		if (m_showCursol != enable)ShowCursor(enable); m_showCursol = enable;
	}

	// 現在の起動モードを取得(他クラスからモードで分岐したい場合用)
	AppMode GetAppMode() const { return m_appMode; }

	// 枠付きウィンドウ⇔モニター全体のボーダレスを切り替える(F11)
	void ToggleBorderless();
	bool IsBorderless() const { return m_borderless; }

	// 自プロセスのいずれかのウィンドウ(ImGui Viewport含む)が前面か
	bool IsAppForeground() const;
	// メインのゲームウィンドウ自体が前面か
	bool IsMainWindowForeground() const { return GetForegroundWindow() == GetWindowHandle(); }

	// クライアント座標/現在のマウス位置を、設計解像度(1280x720)の座標へ変換する
	POINT ClientToVirtual(POINT clientPos) const;
	POINT GetVirtualMousePos() const;

private:

	// 起動時にエディタのみ/プレイのどちらで実行するかを選択させる
	//	KD_EDITOR_ENABLED未定義ビルドでは確認せずPlayを返す
	AppMode SelectStartupMode();

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

	// エディタ表示ON/OFF(F1キー)のエッジ検出用。押した瞬間だけトグルするための前フレーム値
	bool m_prevEditorToggleKeyDown = false;

	bool m_escapeKeyDown = false;

	// ボーダレス切り替え(F11キー)のエッジ検出用前フレーム値
	bool m_prevBorderlessKeyDown = false;

	// ボーダレス状態と、枠付きへ戻すための退避情報
	bool            m_borderless = false;
	LONG_PTR        m_prevStyle = 0;
	WINDOWPLACEMENT m_prevPlacement = { sizeof(WINDOWPLACEMENT) };
	RECT            m_clipRect = {};

	// ゲームウィンドウクラス
	KdWindow		m_window;

	// FPSコントローラー
	KdFPSController	m_fpsController;

	// サウンドマネージャー
	std::unique_ptr<KdAudioManager> m_audioManager = nullptr;

	// ゲーム終了フラグ trueで終了する
	std::atomic<bool>	m_endFlag = false;

	bool				m_showCursol = true;

	// 起動時に選択されたモード(SelectStartupModeで決定され、以降は読み取り専用として扱う)
	AppMode				m_appMode = AppMode::Undecided;

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