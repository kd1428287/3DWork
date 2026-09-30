#pragma once

#include <functional>
#include <imgui.h>

#include "KdPreviewPostProcess.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エディタの「専用カメラ・専用オフスクリーンでミニプレビューを出す」処理の共通部分
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// EffectEditor/MapEditorのRenderPreviewViewport()/DrawPreviewWindow()で重複していた
// 「バッファ生成・退避復元・オービットカメラ・ポストプロセス・ImGui表示」を集約する。
// 「何を描画するか」「注視点」「ウィンドウ上のオーバーレイ」は呼び出し側が渡す。
//
// 使い方：
//	m_preview.Configure(settings);				// コンストラクタ等で1回
//	m_preview.Render(target, [&]{ ...描画... });	// 3D描画パス側から毎フレーム1回
//	m_preview.DrawWindow("Name", 0, [&]{ ... });	// ImGui描画パス側から毎フレーム1回
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class EditorPreviewViewport
{
public:

	struct Settings
	{
		bool	UseMaskRT = false;		// Alphaブレンドのパーティクル用の捨てRT(スロット1)が必要か
		float	FovDeg = 45.0f;
		float	NearZ = 0.05f;
		float	FarZ = 100.0f;

		float	InitialDistance = 3.0f;
		float	MinDistance = 0.2f;
		float	MaxDistance = 50.0f;
		float	WheelSensitivity = 0.3f;

		float	ClearColor[4] = { 0.1f, 0.1f, 0.12f, 1.0f };
	};

	// 設定を適用し、カメラを初期状態に戻す
	void Configure(const Settings& settings);

	// バッファ生成済み(ウィンドウが一度でも開かれてサイズが確定している)か
	bool IsReady() const;

	// target(ワールド座標)を注視点にプレビュー用バッファへ描画する。
	// drawSceneFuncの中では呼び出し側固有の描画だけを行う(RT/カメラの退避・復元は内部で行う)
	void Render(const DirectX::SimpleMath::Vector3& target, const std::function<void()>& drawSceneFunc);

	// ImGuiウィンドウを開き、結果画像の表示と右ドラッグ回転・ホイールズームを処理する。
	// overlayFuncはウィンドウ内(画像表示の直後)で毎回呼ばれる。ギズモやテキスト表示用
	void DrawWindow(const char* windowName, ImGuiWindowFlags flags, const std::function<void()>& overlayFunc = nullptr);

	// ギズモ等で同じカメラ・同じ表示領域を使うための取得口
	DirectX::SimpleMath::Matrix GetViewMatrix(const DirectX::SimpleMath::Vector3& target) const;
	DirectX::SimpleMath::Matrix GetProjMatrix() const;

	int GetWidth() const { return m_viewport.Width; }
	int GetHeight() const { return m_viewport.Height; }
	const ImVec2& GetScreenPos() const { return m_viewport.ScreenPos; }
	const ImVec2& GetScreenSize() const { return m_viewport.ScreenSize; }

private:

	struct Viewport
	{
		std::shared_ptr<KdTexture>	Color;
		std::shared_ptr<KdTexture>	Depth;
		std::shared_ptr<KdTexture>	Mask;	// UseMaskRT時のみ生成

		int		Width = 0;
		int		Height = 0;

		ImVec2	ScreenPos = { 0,0 };
		ImVec2	ScreenSize = { 0,0 };
	};

	struct OrbitCamera
	{
		float Distance = 3.0f;
		float Yaw = 0.0f;
		float Pitch = 0.3f;

		DirectX::SimpleMath::Matrix GetView(const DirectX::SimpleMath::Vector3& target) const;
	};

	// ウィンドウサイズに合わせてバッファを作り直す(サイズ据え置きなら何もしない)
	void Resize(int w, int h);

	Settings				m_settings;
	Viewport				m_viewport;
	OrbitCamera				m_camera;
	KdPreviewPostProcess	m_postProcess;
};
