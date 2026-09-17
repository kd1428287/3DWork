#pragma once

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ゲーム画面(3D描画結果)をオフスクリーンのテクスチャに描画し、
// ImGuiの「Scene」ウィンドウの中に埋め込んで表示するためのクラス
//
// ※エディタ表示の有効/無効は自分では持たない。EditorHostが一元管理する状態を
//   BeginSceneDraw()/DrawSceneWindow()の引数として毎回受け取る(状態の二重管理を避けるため)。
//
// 使い方：
//	・3D描画の直前(EditorHost::BeginSceneDraw()経由)で BeginSceneDraw(editorEnabled) を呼ぶ
//	・ImGui描画パスの中(EditorHost::Draw()内)で DrawSceneWindow(editorEnabled) を呼ぶ
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class EditorViewport
{
public:

	// 3D描画パスの直前に呼ぶ：レンダーターゲットをオフスクリーンに切り替えてクリアする
	// editorEnabled … エディタ表示が有効かどうか(EditorHostが一元管理する状態をそのまま渡す)
	//	falseの場合はオフスクリーンを経由せず、バックバッファへ直接フルスクリーン描画する
	void BeginSceneDraw(bool editorEnabled);

	// ImGui描画パスの中で呼ぶ：「Scene」ウィンドウを描画し、中にオフスクリーンの絵を表示する
	// editorEnabled … falseの場合は何もしない
	void DrawSceneWindow(bool editorEnabled);

	// 現在のオフスクリーンバッファのサイズ
	int GetWidth()  const { return m_width; }
	int GetHeight() const { return m_height; }

	// Sceneウィンドウ内の画像表示領域(スクリーン座標)：ギズモのSetRect等に使用
	const ImVec2& GetScreenPos()  const { return m_screenPos; }
	const ImVec2& GetScreenSize() const { return m_screenSize; }

private:

	// オフスクリーンバッファをサイズ変更(必要な時だけ作り直す)
	void Resize(int w, int h);

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