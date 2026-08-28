#pragma once

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ゲーム画面(3D描画結果)をオフスクリーンのテクスチャに描画し、
// ImGuiの「Scene」ウィンドウの中に埋め込んで表示するためのクラス
//
// 使い方：
//	・3D描画の直前(Application::KdBeginDrawの先頭)で BeginSceneDraw() を呼ぶ
//	・ImGui描画パスの中(KdDebugGUI::GuiProcess()内)で DrawSceneWindow() を呼ぶ
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class EditorViewport
{
public:

	// 3D描画パスの直前に呼ぶ：レンダーターゲットをオフスクリーンに切り替えてクリアする
	// (無効時はバックバッファへ直接フルスクリーン描画する)
	void BeginSceneDraw();

	// ImGui描画パスの中で呼ぶ：「Scene」ウィンドウを描画し、中にオフスクリーンの絵を表示する
	// (無効時は何もしない)
	void DrawSceneWindow();

	// エディタ表示のON/OFF
	void SetEnabled(bool enabled) { m_enabled = enabled; }
	void ToggleEnabled() { m_enabled = !m_enabled; }
	bool IsEnabled() const { return m_enabled; }

	// 現在のオフスクリーンバッファのサイズ
	int GetWidth()  const { return m_width; }
	int GetHeight() const { return m_height; }

	// Sceneウィンドウ内の画像表示領域(スクリーン座標)：ギズモのSetRect等に使用
	const ImVec2& GetScreenPos()  const { return m_screenPos; }
	const ImVec2& GetScreenSize() const { return m_screenSize; }

private:

	// オフスクリーンバッファをサイズ変更(必要な時だけ作り直す)
	void Resize(int w, int h);

	bool	m_enabled = false;	// trueならエディタ(ドッキングUI + Sceneウィンドウ)を表示

	std::shared_ptr<KdTexture>	m_sceneColor = nullptr;	// オフスクリーンのカラーバッファ
	std::shared_ptr<KdTexture>	m_sceneDepth = nullptr;	// オフスクリーンのZバッファ

	int		m_width = 0;
	int		m_height = 0;

	ImVec2	m_screenPos = { 0,0 };	// Sceneウィンドウ内、画像の左上スクリーン座標
	ImVec2	m_screenSize = { 0,0 };	// Sceneウィンドウ内、画像の表示サイズ

	//=====================================================
	// シングルトンパターン
	//=====================================================
private:
	EditorViewport() {}
	~EditorViewport() {}

public:
	static EditorViewport& Instance() {
		static EditorViewport instance;
		return instance;
	}
};