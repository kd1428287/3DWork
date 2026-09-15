#include "Application/main.h"

#include "EditorHost.h"

//#ifdef EDITOR_ENABLED

#include "Common/EditorViewport.h"
#include "Tools/MapEditor.h"
#include "Tools/EffectEditor.h"
#include "Tools/ShaderTuningEditor.h"
#include "Tools/BTEditor.h"

// DockBuilder系APIを使うために必要(公式にも初期配置構築の定番として使われる内部ヘッダ)
// ※旧KdDebugGUI.cppから移設。ドッキングレイアウトはエディタ固有の関心事のためこちらに置く
#include "imgui_internal.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 初期ウィンドウ配置(Unity風)
//	左：Hierarchy(全高) / 中央：Scene(上) / 右：Inspector(全高) / 下：Assets + Log(タブ)
//	imgui.ini に保存された配置が存在しない(=初回起動、またはiniを削除した直後)場合のみ呼ばれる
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static void SetupDefaultDockLayout(ImGuiID dockspaceId)
{
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

	ImGuiID center = dockspaceId;

	// 左：Hierarchy(画面幅の18%、全高)
	ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.18f, nullptr, &center);

	// 右：Inspector(画面幅の22%、全高)
	// ※center は既に左18%分を差し引いた幅になっているため、
	//   画面全体基準で22%になるよう比率を 0.22/(1-0.18) に補正している
	ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.22f / (1.0f - 0.18f), nullptr, &center);

	// 下：Assets + Log(画面高さの30%) 、残った部分がScene(中央上)
	ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.30f, nullptr, &center);

	ImGui::DockBuilderDockWindow("Hierarchy", left);
	ImGui::DockBuilderDockWindow("Inspector", right);
	ImGui::DockBuilderDockWindow("Scene", center);
	ImGui::DockBuilderDockWindow("Assets", bottom);
	ImGui::DockBuilderDockWindow("Log Window", bottom);	// Assetsと同じノードなのでタブ化される

	ImGui::DockBuilderFinish(dockspaceId);
}

void EditorHost::Init(int w, int h)
{
	KdDebugGUI::Instance().GuiInit(w, h);
}

bool EditorHost::IsViewportEnabled() const
{
	return EditorViewport::Instance().IsEnabled();
}

void EditorHost::RenderPreviewViewports()
{
	// エフェクトプレビュー専用ビューポートへの描画
	EffectEditor::Instance().RenderPreviewViewport();
	// マッププレビュー
	MapEditor::Instance().RenderPreviewViewport();
}

void EditorHost::Draw()
{
	// 初期化されてないなら動作させない(KdDebugGUI側で判定・無視される)
	KdDebugGUI::Instance().BeginFrame();

	// エディタ表示中のみ、ドッキングUI一式(Hierarchy/Inspector/Assets/Scene/Log)を描画する
	// "Pause"入力でON/OFF切替(main.cpp の Execute() 内を参照)
	if (IsViewportEnabled())
	{
		// 画面全体を覆うドックスペースの土台
		ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");

		// このIDのノードがまだ存在しない(=imgui.iniに保存された配置が無い)場合のみ、
		// Unity風の既定レイアウトを構築する。2回目以降はユーザーが動かした配置がそのまま復元される
		if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr)
		{
			SetupDefaultDockLayout(dockspaceId);
		}

		ImGui::DockSpaceOverViewport(dockspaceId, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

		// ゲーム画面を表示するSceneウィンドウ(中身はオフスクリーンに描画されたゲーム画面)
		EditorViewport::Instance().DrawSceneWindow();

		// マップエディタ
		MapEditor::Instance().Update();

		// シェーダーエディタ
		ShaderTuningEditor::Instance().Update();

		// エフェクトエディタ
		EffectEditor::Instance().Update();

		// BTエディタ
		BTEditor::Instance().Update();
	}

	// ここより上にImGuiの描画はする事(EndFrame内でImGui::Render()を呼ぶため)
	KdDebugGUI::Instance().EndFrame();
}

void EditorHost::Release()
{
	KdDebugGUI::Instance().GuiRelease();
}

//#endif  EDITOR_ENABLED