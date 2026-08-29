#include "../../../Application/main.h"

#include "KdDebugGUI.h"
#include "../../../Application/Editor/MapEditor.h"
#include "../../../Application/Editor/EditorViewport.h"
#include "../../../Application/Editor/EffectEditor.h"

// DockBuilder系APIを使うために必要(公式にも初期配置構築の定番として使われる内部ヘッダ)
#include "imgui_internal.h"

KdDebugGUI::KdDebugGUI()
{}
KdDebugGUI::~KdDebugGUI()
{ 
	GuiRelease(); 
}

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

	ImGui::DockBuilderDockWindow("Hierarchy",  left);
	ImGui::DockBuilderDockWindow("Effect Hierarchy", left);	// Hierarchyとタブ化

	ImGui::DockBuilderDockWindow("Inspector",  right);
	ImGui::DockBuilderDockWindow("Effect Inspector", right);	// Inspectorとタブ化

	ImGui::DockBuilderDockWindow("Scene",      center);

	ImGui::DockBuilderDockWindow("Assets",     bottom);
	ImGui::DockBuilderDockWindow("Effect Assets", bottom);	// Assetsとタブ化
	ImGui::DockBuilderDockWindow("Log Window", bottom);	// Assetsと同じノードなのでタブ化される

	ImGui::DockBuilderFinish(dockspaceId);
}

void KdDebugGUI::GuiInit(int w, int h)
{
	// 初期化済みなら動作させない
	if (m_uqLog) return;

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	// ドッキング機能を有効化(要 Dear ImGui docking ブランチ)

	// Setup Dear ImGui style
	// ImGui::StyleColorsDark();
	ImGui::StyleColorsClassic();

	// Setup Platform/Renderer bindings
	// ※ ImGui_ImplWin32_Init は本来 hwnd 一つだけを引数に取る関数のため、
	//    第二引数(サイズ)は渡さない
	ImGui_ImplWin32_Init(Application::Instance().GetWindowHandle());
	ImGui_ImplDX11_Init(KdDirect3D::Instance().WorkDev(), KdDirect3D::Instance().WorkDevContext());

#include "imgui/ja_glyph_ranges.h"
	ImFontConfig config;
	config.MergeMode = true;
	//応急措置
	config.MergeMode = false;
	io.Fonts->AddFontDefault();
	// 日本語対応
	io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\msgothic.ttc", 13.0f, &config, glyphRangesJapanese);
	m_uqLog = std::make_unique<ImGuiAppLog>();
}

void KdDebugGUI::GuiProcess()
{
	// 初期化されてないなら動作させない
	if (!m_uqLog) return;

	//===========================================================
	// ImGui開始
	//===========================================================
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	//===========================================================
	// 以下にImGui描画処理を記述
	//===========================================================

	// エディタ表示中のみ、ドッキングUI一式(Hierarchy/Inspector/Assets/Scene/Log)を描画する
	// "Pause"入力でON/OFF切替(main.cpp の Execute() 内を参照)
	if (EditorViewport::Instance().IsEnabled())
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

		// ログウィンドウ
		m_uqLog->Draw("Log Window");

		//=====================================================
		// ログ出力 ・・・ AddLog("～") で追加
		//=====================================================

	//	m_uqLog->AddLog("hello world\n");

		//=====================================================
		// 別ソースファイルからログを出力する場合
		//=====================================================

	//	KdDebugGUI::Instance().AddLog("TestLog\n");

		// ゲーム画面を表示するSceneウィンドウ(中身はオフスクリーンに描画されたゲーム画面)
		EditorViewport::Instance().DrawSceneWindow();

		// マップエディタ(Hierarchy / Inspector / Assets / ギズモ)
		MapEditor::Instance().Update();

		// エフェクト配置エディタ(Effect Hierarchy / Effect Inspector / Effect Assets / ギズモ)
		//	※ImGuizmo::BeginFrame()はMapEditor::Update()内で既に呼ばれているため、
		//	  EffectEditor::Update()内では呼ばない
		EffectEditor::Instance().Update();
	}

	//===========================================================
	// ここより上にImGuiの描画はする事
	//===========================================================
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void KdDebugGUI::AddLog(const char* fmt,...)
{
	// 初期化されてないなら動作させない
	if (!m_uqLog) return;

	char tmpStr[128] = {};
	va_list args;
	va_start(args, fmt);
	vsprintf_s(tmpStr, fmt, args);
	m_uqLog->AddLog(tmpStr);
	va_end(args);
}

void KdDebugGUI::ClearLog()
{
	// 初期化されてないなら動作させない
	if (!m_uqLog) return;

	m_uqLog->Clear();
}

void KdDebugGUI::GuiRelease()
{
	// 初期化されてないなら動作させない
	if (!m_uqLog) return;

	m_uqLog = nullptr;

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}
