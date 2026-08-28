#include "../main.h"

#include "EditorViewport.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Sceneウィンドウの表示サイズに合わせてオフスクリーンバッファを作り直す
//	KdDirect3D::Init() 内のZバッファ作成と同じ設定を踏襲しています
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EditorViewport::Resize(int w, int h)
{
	if (w <= 0 || h <= 0) return;

	// サイズが変わっていなければ作り直さない
	if (w == m_width && h == m_height && m_sceneColor && m_sceneDepth) return;

	m_width = w;
	m_height = h;

	// ----- カラーバッファ(ImGui::Imageで表示するためSHADER_RESOURCEも付与) -----
	{
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.Width = (UINT)w;
		desc.Height = (UINT)h;
		desc.CPUAccessFlags = 0;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		m_sceneColor = std::make_shared<KdTexture>();
		m_sceneColor->Create(desc);
	}

	// ----- Zバッファ(KdDirect3D::Init内のZバッファ作成と同一設定) -----
	{
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		desc.Width = (UINT)w;
		desc.Height = (UINT)h;
		desc.CPUAccessFlags = 0;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		m_sceneDepth = std::make_shared<KdTexture>();
		m_sceneDepth->Create(desc);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 3D描画の描画先をオフスクリーンに切り替える
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EditorViewport::BeginSceneDraw()
{
	ID3D11DeviceContext* context = KdDirect3D::Instance().WorkDevContext();

	//-------------------------------------------------------
	// エディタ無効時：オフスクリーンを経由せず、バックバッファへ直接フルスクリーン描画する
	//-------------------------------------------------------
	if (!m_enabled)
	{
		KdDirect3D::Instance().ClearBackBuffer();

		ID3D11RenderTargetView* rtvs[] = { KdDirect3D::Instance().WorkBackBuffer()->WorkRTView() };
		context->OMSetRenderTargets(1, rtvs, KdDirect3D::Instance().WorkZBuffer()->WorkDSView());

		RECT rect;
		GetClientRect(Application::Instance().GetWindowHandle(), &rect);

		D3D11_VIEWPORT vp = {};
		vp.Width = (float)(rect.right - rect.left);
		vp.Height = (float)(rect.bottom - rect.top);
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		context->RSSetViewports(1, &vp);
		return;
	}

	//-------------------------------------------------------
	// エディタ有効時：オフスクリーン(Sceneウィンドウ用バッファ)に切り替え
	//-------------------------------------------------------

	// Sceneウィンドウがまだ一度も描画されておらずサイズ不明な場合は何もしない
	if (!m_sceneColor || !m_sceneDepth) return;

	// レンダーターゲット・Zバッファをオフスクリーンに切り替え
	ID3D11RenderTargetView* rtvs[] = { m_sceneColor->WorkRTView() };
	context->OMSetRenderTargets(1, rtvs, m_sceneDepth->WorkDSView());

	// クリア
	static const float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	context->ClearRenderTargetView(m_sceneColor->WorkRTView(), clearColor);
	context->ClearDepthStencilView(m_sceneDepth->WorkDSView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// ビューポートもオフスクリーンのサイズに合わせる
	D3D11_VIEWPORT vp = {};
	vp.Width = (float)m_width;
	vp.Height = (float)m_height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	context->RSSetViewports(1, &vp);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Sceneウィンドウの描画：中にオフスクリーンの絵をImGui::Imageで表示する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EditorViewport::DrawSceneWindow()
{
	if (!m_enabled) return;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("Scene");

	ImVec2 regionSize = ImGui::GetContentRegionAvail();

	// ウィンドウサイズが変わったらオフスクリーンバッファを作り直す
	if (regionSize.x >= 1.0f && regionSize.y >= 1.0f)
	{
		Resize((int)regionSize.x, (int)regionSize.y);
	}

	if (m_sceneColor)
	{
		m_screenPos = ImGui::GetCursorScreenPos();
		m_screenSize = regionSize;

		// ※ KdTextureがSRVを保持している前提。実際のアクセサ名が違う場合はここを合わせてください
		ImGui::Image((ImTextureID)m_sceneColor->WorkSRView(), regionSize);
	}

	ImGui::End();
	ImGui::PopStyleVar();
}