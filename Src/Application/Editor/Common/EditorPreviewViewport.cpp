#include "Application/main.h"

#include "EditorPreviewViewport.h"

void EditorPreviewViewport::Configure(const Settings& settings)
{
	m_settings = settings;
	m_camera.Distance = settings.InitialDistance;
	m_camera.Pan = { 0, 0, 0 };
}

bool EditorPreviewViewport::IsReady() const
{
	if (!m_viewport.Color || !m_viewport.Depth) { return false; }
	if (m_settings.UseMaskRT && !m_viewport.Mask) { return false; }
	if (m_settings.UseBrightRT && !m_viewport.Bright) { return false; }
	if (m_settings.UseEffectLayerRT && !m_viewport.Effect) { return false; }
	return m_viewport.Width > 0 && m_viewport.Height > 0;
}

DirectX::SimpleMath::Matrix EditorPreviewViewport::OrbitCamera::GetView(const DirectX::SimpleMath::Vector3& target) const
{
	using namespace DirectX::SimpleMath;

	float cosPitch = cosf(Pitch);
	Vector3 offset(
		Distance * cosPitch * sinf(Yaw),
		Distance * sinf(Pitch),
		Distance * cosPitch * cosf(Yaw));

	const Vector3 center = target + Pan;
	return Matrix::CreateLookAt(center + offset, center, Vector3::Up);
}

void EditorPreviewViewport::OrbitCamera::GetBasis(DirectX::SimpleMath::Vector3& right,
	DirectX::SimpleMath::Vector3& up, DirectX::SimpleMath::Vector3& forward) const
{
	using namespace DirectX::SimpleMath;

	const float cosPitch = cosf(Pitch);
	const Vector3 z(cosPitch * sinf(Yaw), sinf(Pitch), cosPitch * cosf(Yaw));	// 注視点→カメラ

	right = Vector3::Up.Cross(z);
	right.Normalize();
	up = z.Cross(right);
	forward = -z;
}

DirectX::SimpleMath::Matrix EditorPreviewViewport::GetViewMatrix(const DirectX::SimpleMath::Vector3& target) const
{
	return m_camera.GetView(target);
}

DirectX::SimpleMath::Matrix EditorPreviewViewport::GetProjMatrix() const
{
	const float aspect = (m_viewport.Height > 0) ? (float)m_viewport.Width / (float)m_viewport.Height : 1.0f;

	return DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(
		DirectX::XMConvertToRadians(m_settings.FovDeg), aspect, m_settings.NearZ, m_settings.FarZ);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// バッファ(Color/Depth/必要ならMask)の作り直し
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EditorPreviewViewport::Resize(int w, int h)
{
	if (w <= 0 || h <= 0) { return; }

	// サイズが変わっておらず、必要なバッファも揃っていれば作り直さない
	const bool maskOk = !m_settings.UseMaskRT || m_viewport.Mask;
	const bool brightOk = !m_settings.UseBrightRT || m_viewport.Bright;
	const bool effectOk = !m_settings.UseEffectLayerRT || m_viewport.Effect;
	if (w == m_viewport.Width && h == m_viewport.Height && m_viewport.Color && m_viewport.Depth && maskOk && brightOk && effectOk) { return; }

	m_viewport.Width = w;
	m_viewport.Height = h;

	auto makeDesc = [&](DXGI_FORMAT format, UINT bindFlags)
		{
			D3D11_TEXTURE2D_DESC desc = {};
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.Format = format;
			desc.BindFlags = bindFlags;
			desc.Width = (UINT)w;
			desc.Height = (UINT)h;
			desc.CPUAccessFlags = 0;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			return desc;
		};

	m_viewport.Color = std::make_shared<KdTexture>();
	m_viewport.Color->Create(makeDesc(DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE));

	m_viewport.Depth = std::make_shared<KdTexture>();
	m_viewport.Depth->Create(makeDesc(DXGI_FORMAT_R24G8_TYPELESS, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE));

	// カラーグレード除外マスク書き込み用の捨てRT。中身は使わないが、Alphaブレンドの
	// パーティクル(m_PS_Masked)がSV_Target1へ書き込めるようスロット1に何かバインドしておく必要がある
	if (m_settings.UseMaskRT)
	{
		m_viewport.Mask = std::make_shared<KdTexture>();
		m_viewport.Mask->Create(makeDesc(DXGI_FORMAT_R8_UNORM, D3D11_BIND_RENDER_TARGET));
	}

	// Bloom元となるBright専用RT(加算描画のみ。SRVも持たせてGenerateBlurTexture()の入力に使う)
	if (m_settings.UseBrightRT)
	{
		m_viewport.Bright = std::make_shared<KdTexture>();
		m_viewport.Bright->Create(makeDesc(DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE));
	}

	// エフェクト層(カラーグレードの対象外。1を超える明るさも保てるようHDR形式)
	if (m_settings.UseEffectLayerRT)
	{
		m_viewport.Effect = std::make_shared<KdTexture>();
		m_viewport.Effect->Create(makeDesc(DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE));
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 専用カメラ・専用バッファへの描画。RT/ビューポート/カメラCBは退避し、終了時に復元する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EditorPreviewViewport::Render(const DirectX::SimpleMath::Vector3& target,
	const std::function<void()>& drawSceneFunc,
	const std::function<void()>& drawBrightFunc,
	const std::function<void()>& drawEffectFunc)
{
	if (!IsReady()) { return; }

	ID3D11DeviceContext* context = KdDirect3D::Instance().WorkDevContext();

	// 退避(RTVは2スロットぶん保持し、復元時に元の束縛状態をそのまま戻す)
	KdShaderManager::cbCamera savedCamera = KdShaderManager::Instance().GetCameraCB();
	const KdShaderManager::cBright savedLight = KdShaderManager::Instance().GetLightCB();

	ID3D11RenderTargetView* savedRTVs[2] = { nullptr, nullptr };
	ID3D11DepthStencilView* savedDSV = nullptr;
	context->OMGetRenderTargets(2, savedRTVs, &savedDSV);

	UINT savedVPNum = 1;
	D3D11_VIEWPORT savedVP = {};
	context->RSGetViewports(&savedVPNum, &savedVP);

	// プレビュー用バッファへ切り替え・クリア
	ID3D11RenderTargetView* rtvs[2] = { m_viewport.Color->WorkRTView(), nullptr };
	UINT rtvCount = 1;
	if (m_settings.UseMaskRT)
	{
		rtvs[1] = m_viewport.Mask->WorkRTView();
		rtvCount = 2;
	}
	context->OMSetRenderTargets(rtvCount, rtvs, m_viewport.Depth->WorkDSView());

	context->ClearRenderTargetView(m_viewport.Color->WorkRTView(), m_settings.ClearColor);
	if (m_settings.UseMaskRT)
	{
		context->ClearRenderTargetView(m_viewport.Mask->WorkRTView(), kBlackColor);
	}
	if (m_settings.UseBrightRT)
	{
		context->ClearRenderTargetView(m_viewport.Bright->WorkRTView(), kBlackColor);
	}
	if (m_settings.UseEffectLayerRT)
	{
		const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };	// RGBもAも0=エフェクト無し
		context->ClearRenderTargetView(m_viewport.Effect->WorkRTView(), zero);
	}
	context->ClearDepthStencilView(m_viewport.Depth->WorkDSView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	D3D11_VIEWPORT vp = {};
	vp.Width = (float)m_viewport.Width;
	vp.Height = (float)m_viewport.Height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	context->RSSetViewports(1, &vp);

	// プレビュー用カメラを適用して、呼び出し側固有の描画を行う
	DirectX::SimpleMath::Matrix view = m_camera.GetView(target);
	KdShaderManager::Instance().WriteCBCamera(view.Invert(), GetProjMatrix());

	// プレビュー専用のライト。影範囲は注視点中心(本編はカメラ位置中心のため、そのままでは範囲外になりうる)
	if (m_settings.OverrideLight)
	{
		KdShaderManager::cBright light;

		DirectX::SimpleMath::Vector3 dir = m_settings.LightDir;
		dir.Normalize();

		const DirectX::SimpleMath::Vector3 center = target + m_camera.Pan;
		const float size = m_settings.ShadowAreaSize;

		light.DirLight_Dir = dir;
		light.DirLight_mVP = DirectX::XMMatrixLookAtLH(center - dir * size, center, DirectX::SimpleMath::Vector3::Up)
			* DirectX::XMMatrixOrthographicLH(size, size, 0.0f, size * 2.0f);

		KdShaderManager::Instance().WriteCBLight(light);
	}

	if (drawSceneFunc) { drawSceneFunc(); }

	// エフェクト層(カラーグレードの対象外)：専用RTへ描き、Apply()で最後に重ねる。深度はColor側と共有
	if (m_settings.UseEffectLayerRT && drawEffectFunc)
	{
		ID3D11RenderTargetView* effectRTV = m_viewport.Effect->WorkRTView();
		context->OMSetRenderTargets(1, &effectRTV, m_viewport.Depth->WorkDSView());

		KdPostProcessShader& postProcess = KdShaderManager::Instance().m_postProcessShader;
		postProcess.SetExternalEffectLayerActive(true);
		drawEffectFunc();
		postProcess.SetExternalEffectLayerActive(false);
	}

	// Bloom元となるBright専用RTへの加算描画(本編のBeginBright()/EndBright()と同じ考え方)。
	// 深度は共有(既に描いたColor側と同じ被写体を対象にするので、Zバッファは引き継いでよい)
	if (m_settings.UseBrightRT && drawBrightFunc)
	{
		ID3D11RenderTargetView* brightRTVs[2] = { m_viewport.Bright->WorkRTView(), nullptr };
		UINT brightRTVCount = 1;
		if (m_settings.UseMaskRT)
		{
			brightRTVs[1] = m_viewport.Mask->WorkRTView();
			brightRTVCount = 2;
		}
		context->OMSetRenderTargets(brightRTVCount, brightRTVs, m_viewport.Depth->WorkDSView());

		KdShaderManager::Instance().ChangeBlendState(KdBlendState::Add);
		KdShaderManager::Instance().ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);

		drawBrightFunc();

		KdShaderManager::Instance().UndoDepthStencilState();
		KdShaderManager::Instance().UndoBlendState();
	}

	// 復元
	KdShaderManager::Instance().WriteCBCamera(savedCamera.mView.Invert(), savedCamera.mProj);

	if (m_settings.OverrideLight) { KdShaderManager::Instance().WriteCBLight(savedLight); }

	context->OMSetRenderTargets(2, savedRTVs, savedDSV);
	if (savedRTVs[0]) { savedRTVs[0]->Release(); }
	if (savedRTVs[1]) { savedRTVs[1]->Release(); }
	if (savedDSV) { savedDSV->Release(); }

	context->RSSetViewports(savedVPNum, &savedVP);

	// 描画が完全に終わった後にBloom→カラーグレードを適用する
	// ※Apply()内部は自前のRT/ビューポート退避・復元を行うため、ここでの追加の後始末は不要
	m_postProcess.Apply(m_viewport.Color, m_settings.UseBrightRT ? m_viewport.Bright : nullptr,
		m_settings.UseEffectLayerRT ? m_viewport.Effect : nullptr);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ImGuiウィンドウへの表示と、右ドラッグ回転・ホイールズームの入力処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EditorPreviewViewport::DrawWindow(const char* windowName, ImGuiWindowFlags flags, const std::function<void()>& overlayFunc)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin(windowName, nullptr, flags);

	ImVec2 regionSize = ImGui::GetContentRegionAvail();

	// ウィンドウサイズが変わったらオフスクリーンバッファを作り直す
	if (regionSize.x >= 1.0f && regionSize.y >= 1.0f)
	{
		Resize((int)regionSize.x, (int)regionSize.y);
	}

	if (m_viewport.Color)
	{
		m_viewport.ScreenPos = ImGui::GetCursorScreenPos();
		m_viewport.ScreenSize = regionSize;

		// カラーグレード適用後の結果を表示する。リサイズ直後で結果がまだ無い場合のみ、
		// 生のプレビュー画像にフォールバックする(次フレームには結果が揃う)
		const std::shared_ptr<KdTexture>& displayTex =
			m_postProcess.GetResultTexture() ? m_postProcess.GetResultTexture() : m_viewport.Color;

		ImGui::Image((ImTextureID)displayTex->WorkSRView(), regionSize);

		// 右ドラッグ：回転 / 右ホールド中のWASD・Q/E：前後左右・下上へ移動(Shiftで加速)
		// 中ドラッグ：画面平行移動 / ホイール：ズーム / F：移動量を戻す
		ImGuiIO& io = ImGui::GetIO();
		const bool hovered = ImGui::IsItemHovered();

		if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) { m_rightHeld = true; }
		if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) { m_middleHeld = true; }
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) { m_rightHeld = false; }
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) { m_middleHeld = false; }

		DirectX::SimpleMath::Vector3 right, up, forward;
		m_camera.GetBasis(right, up, forward);

		if (m_rightHeld)
		{
			m_camera.Yaw -= io.MouseDelta.x * 0.01f;
			m_camera.Pitch += io.MouseDelta.y * 0.01f;
			m_camera.Pitch = std::clamp(m_camera.Pitch, -1.5f, 1.5f);

			DirectX::SimpleMath::Vector3 move(0, 0, 0);
			if (ImGui::IsKeyDown(ImGuiKey_W)) { move += forward; }
			if (ImGui::IsKeyDown(ImGuiKey_S)) { move -= forward; }
			if (ImGui::IsKeyDown(ImGuiKey_D)) { move += right; }
			if (ImGui::IsKeyDown(ImGuiKey_A)) { move -= right; }
			if (ImGui::IsKeyDown(ImGuiKey_E)) { move += DirectX::SimpleMath::Vector3::Up; }
			if (ImGui::IsKeyDown(ImGuiKey_Q)) { move -= DirectX::SimpleMath::Vector3::Up; }

			if (move.LengthSquared() > 0.0f)
			{
				move.Normalize();
				const float speed = std::max(m_camera.Distance, 1.0f) * m_settings.FlySpeedScale * (io.KeyShift ? 3.0f : 1.0f);
				m_camera.Pan += move * speed * io.DeltaTime;
			}
		}

		if (m_middleHeld)
		{
			// 画面1ピクセルあたりのワールド距離(注視点の位置での値)
			const float worldPerPixel = (m_viewport.Height > 0)
				? 2.0f * m_camera.Distance * tanf(DirectX::XMConvertToRadians(m_settings.FovDeg) * 0.5f) / (float)m_viewport.Height
				: 0.0f;

			m_camera.Pan -= right * (io.MouseDelta.x * worldPerPixel);
			m_camera.Pan += up * (io.MouseDelta.y * worldPerPixel);
		}

		if (hovered)
		{
			if (io.MouseWheel != 0.0f)
			{
				m_camera.Distance -= io.MouseWheel * m_settings.WheelSensitivity;
				m_camera.Distance = std::clamp(m_camera.Distance, m_settings.MinDistance, m_settings.MaxDistance);
			}

			if (!m_rightHeld && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F, false)) { ResetPan(); }
		}
	}

	if (overlayFunc) { overlayFunc(); }

	ImGui::End();
	ImGui::PopStyleVar();
}