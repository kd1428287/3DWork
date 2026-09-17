#include "Application/main.h"

#include "EditorHost.h"

#ifdef EDITOR_ENABLED

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

void EditorHost::BeginSceneDraw()
{
	// EditorViewportは自分では有効/無効を持たない。EditorHostが一元管理するm_enabledを
	// そのまま渡すことで、「今エディタ表示中かどうか」の情報源を一箇所に集約している
	EditorViewport::Instance().BeginSceneDraw(m_enabled);
}

void EditorHost::RenderPreviewViewports()
{
	// 無効な間は、プレビュー描画のコストも払わない
	if (!m_enabled) { return; }

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
	// (F1キーでのトグル。main.cpp Execute()内を参照)
	if (m_enabled)
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
		EditorViewport::Instance().DrawSceneWindow(m_enabled);

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
	// BTEditor::Shutdown()は「ImGui本体が破棄される前に明示的に呼ぶこと」という制約があるため
	// (BTEditor.h参照)、ImGuiコンテキストを破棄するKdDebugGUI::GuiRelease()より必ず先に呼ぶ
	BTEditor::Instance().Shutdown();

	KdDebugGUI::Instance().GuiRelease();
}

#endif // KD_EDITOR_ENABLED