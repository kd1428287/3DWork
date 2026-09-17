#pragma once

//============================================================
// EditorHost
//	・エディタ関連(EditorViewport / MapEditor / EffectEditor / ShaderTuningEditor / BTEditor)への
//	  唯一の窓口。Applicationやmain.cppはこのクラス以外のエディタ関連クラスを直接知らない。
//	・KdDebugGUIはImGuiのインフラ(Context生成/破棄、フレーム境界)のみを提供し、
//	  「何を描くか(ドッキングレイアウト、各ツールのUpdate)」の判断は全てこちらが持つ。
//	  → KdDebugGUI ⇔ EditorHost の相互参照を解消するための設計。
//	・「エディタ表示を行うかどうか」の状態は、このクラスだけが保持する唯一の情報源(Single
//	  Source of Truth)。EditorViewport等の個別クラスは自分では有効/無効を持たず、
//	  EditorHostから渡された値をその都度使うだけ、という一方向の依存にしてある。
//============================================================

#ifdef EDITOR_ENABLED

class EditorHost
{
public:
	static EditorHost& Instance() { static EditorHost i; return i; }

	//=====================================================
	// エディタ表示の有効/無効(唯一のスイッチ)
	//	・falseの間は、Draw()内のドッキングUI一式とRenderPreviewViewports()
	//	  (エフェクト/マッププレビュー)が丸ごとスキップされ、BeginSceneDraw()も
	//	  ゲーム画面をオフスクリーンを経由せずバックバッファへ直接描画する側に回る。
	//	  (ImGui自体のBeginFrame/EndFrameは継続して呼ばれるため、ImGui内部状態が壊れる心配はない)
	//=====================================================
	void SetEnabled(bool enabled) { m_enabled = enabled; }
	bool IsEnabled() const { return m_enabled; }
	void ToggleEnabled() { m_enabled = !m_enabled; }

	// アプリケーション初期化時に一度だけ呼ぶ(内部でKdDebugGUI::GuiInitを呼ぶ)
	void Init(int w, int h);

	// 3D描画開始時に毎フレーム呼ぶ(KdBeginDraw()から)
	//	IsEnabled()の値に応じて、EditorViewportがオフスクリーンに描くかバックバッファに
	//	直接描くかを切り替える
	void BeginSceneDraw();

	// メインループの描画パスから呼ぶ(Draw()の後、PostDraw()の前)
	//	旧: main.cpp Execute()内で EffectEditor/MapEditor の RenderPreviewViewport() を個別に呼んでいた処理
	void RenderPreviewViewports();

	// KdPostDraw()から呼ぶ。エディタ用ドッキングUI一式の描画一式(ImGui NewFrame〜Render〜マルチビューポート処理まで)
	//	旧: KdDebugGUI::GuiProcess()
	void Draw();

	// アプリケーション終了時に呼ぶ(内部でBTEditor::Shutdown→KdDebugGUI::GuiReleaseの順で呼ぶ)
	void Release();

private:
	EditorHost() {}

	bool m_enabled = false;	
};

#else

// エディタ非搭載ビルド用のダミー実装(Nullオブジェクトパターン)
//	KD_EDITOR_ENABLEDを定義しないビルド構成(出荷相当ビルド)では、
//	このクラスの中身は空になり、エディタ関連コードは一切バイナリに含まれない。
//	呼び出し側(Application等)はこの切り替えを意識する必要が無い。
class EditorHost
{
public:
	static EditorHost& Instance() { static EditorHost i; return i; }

	void SetEnabled(bool) {}
	bool IsEnabled() const { return false; }
	void ToggleEnabled() {}

	void Init(int, int) {}
	// 何もしない = デバイス初期化時点の既定レンダーターゲット(バックバッファ)へそのまま描画される想定
	void BeginSceneDraw() {}
	void RenderPreviewViewports() {}
	void Draw() {}
	void Release() {}

private:
	EditorHost() {}
};

#endif