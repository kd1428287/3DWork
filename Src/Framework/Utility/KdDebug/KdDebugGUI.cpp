#include "Application/main.h"

#include "KdDebugGUI.h"

// ※EditorHost/EditorViewport/各エディタツールのincludeは不要になった
//   (このクラスは「何を描くか」を一切知らないため)

KdDebugGUI::KdDebugGUI()
{}
KdDebugGUI::~KdDebugGUI()
{
	// 静的破棄順に依存させないため、本来はApplication::Release()等から
	// GuiRelease()を明示的に呼ぶこと。デストラクタでの呼び出しは保険(GuiReleaseは多重呼び出し安全)
	GuiRelease();
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
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;	// マルチビューポート機能を有効化(ウィンドウを画面外に出すと別OSウィンドウになる)

	// Setup Dear ImGui style
	// ImGui::StyleColorsDark();
	ImGui::StyleColorsClassic();

	// Setup Platform/Renderer bindings
	// ※ ImGui_ImplWin32_Init は本来 hwnd 一つだけを引数に取る関数のため、
	//    第二引数(サイズ)は渡さない
	ImGui_ImplWin32_Init(Application::Instance().GetWindowHandle());
	ImGui_ImplDX11_Init(KdDirect3D::Instance().WorkDev(), KdDirect3D::Instance().WorkDevContext());

#include "imgui/ja_glyph_ranges.h"

	ImFontConfig configDefault;
	configDefault.SizePixels = 13.0f; // ← 明示的に指定(マージ側と揃える)
	io.Fonts->AddFontDefault(&configDefault);

	ImFontConfig config;
	config.MergeMode = true;
	//io.Fonts->AddFontDefault();
	// 日本語対応
	io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\msgothic.ttc", 13.0f, &config, glyphRangesJapanese);
	m_uqLog = std::make_unique<ImGuiAppLog>();
}

void KdDebugGUI::BeginFrame()
{
	// 初期化されてないなら動作させない
	if (!m_uqLog) return;

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void KdDebugGUI::EndFrame()
{
	// 初期化されてないなら動作させない
	if (!m_uqLog) return;

	//===========================================================
	// ここより上にImGuiの描画はする事
	//===========================================================
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// マルチビューポート：ドッキングウィンドウを画面外にドラッグして分離した「別ウィンドウ」の更新・描画
	//	(メインウィンドウの描画とは別に、ここでまとめて処理する)
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

void KdDebugGUI::AddLog(const char* fmt, ...)
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