#pragma once

//============================================================
// EditorHost
//	・エディタ関連(EditorViewport / MapEditor / EffectEditor / ShaderTuningEditor / BTEditor)への
//	  唯一の窓口。Applicationやmain.cppはこのクラス以外のエディタ関連クラスを直接知らない。
//	・KdDebugGUIはImGuiのインフラ(Context生成/破棄、フレーム境界)のみを提供し、
//	  「何を描くか(ドッキングレイアウト、各ツールのUpdate)」の判断は全てこちらが持つ。
//	  → KdDebugGUI ⇔ EditorHost の相互参照を解消するための設計。
//============================================================

#ifdef EDITOR_ENABLED

class EditorHost
{
public:
	static EditorHost& Instance() { static EditorHost i; return i; }

	// アプリケーション初期化時に一度だけ呼ぶ(内部でKdDebugGUI::GuiInitを呼ぶ)
	void Init(int w, int h);

	// 3D描画開始時に毎フレーム呼ぶ(KdBeginDraw()から)
	//	内部でEditorViewportが「オフスクリーンに描くか/バックバッファに直接描くか」を切り替える
	void BeginSceneDraw();

	// メインループの描画パスから呼ぶ(Draw()の後、PostDraw()の前)
	//	旧: main.cpp Execute()内で EffectEditor/MapEditor の RenderPreviewViewport() を個別に呼んでいた処理
	void RenderPreviewViewports();

	// KdPostDraw()から呼ぶ。エディタ用ドッキングUI一式の描画一式(ImGui NewFrame〜Render〜マルチビューポート処理まで)
	//	旧: KdDebugGUI::GuiProcess()
	void Draw();

	// EditorViewportが有効(エディタ表示中)かどうか
	//	旧: main.cpp / KdDebugGUI.cpp から EditorViewport::Instance().IsEnabled() を直接参照していた箇所
	bool IsViewportEnabled() const;

	// アプリケーション終了時に呼ぶ(内部でKdDebugGUI::GuiReleaseを呼ぶ)
	void Release();

private:
	EditorHost() {}
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

	void Init(int, int) {}
	// 何もしない = デバイス初期化時点の既定レンダーターゲット(バックバッファ)へそのまま描画される想定
	void BeginSceneDraw() {}
	void RenderPreviewViewports() {}
	void Draw() {}
	bool IsViewportEnabled() const { return false; }
	void Release() {}

private:
	EditorHost() {}
};

#endif