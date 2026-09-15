#pragma once

class EditorHost
{
public:
	static EditorHost& Instance() { static EditorHost i; return i; }

	void Init(int w, int h);
	void Update();                 // 各エディタツールのUpdate集約（旧GuiProcess内の中身）
	void RenderPreviewViewports(); // EffectEditor/MapEditorのプレビュー描画
	bool IsViewportEnabled() const;
	void Release();

private:
	EditorHost() {}
};