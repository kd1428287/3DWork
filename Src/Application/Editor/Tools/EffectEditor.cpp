#include "Application/main.h"

#include "EffectEditor.h"
#include "../Common/EditorViewport.h"
#include "Application/Definitions/Loaders/EffectDataLoader.h"
#include "../../Effect/Particle/EffectDispatcher.h"

#include "imgui_internal.h"

#include <filesystem>

static const std::string kTextureAssetRoot = "Asset/Textures/Game/Effect/";

DirectX::SimpleMath::Matrix EffectObject::GetMatrix() const
{
	float m[16];
	ImGuizmo::RecomposeMatrixFromComponents(&pos.x, &rotate.x, &scale.x, m);

	DirectX::SimpleMath::Matrix mat;
	memcpy(&mat, m, sizeof(float) * 16);
	return mat;
}

// EffectEditor専用ドックスペースの初期レイアウト
//	左：Effect Hierarchy / 中央：Effect Inspector・右：Effect Preview / 下：Effect Editor(メニュー) + Effect Assets(タブ)
static void SetupEffectDockLayout(ImGuiID dockspaceId, const ImVec2& size)
{
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceId, size);

	ImGuiID center = dockspaceId;

	// 左：Effect Hierarchy(幅30%)
	ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.30f, nullptr, &center);

	// 下：Effect Editor(メニュー) + Effect Assets(タブ)、高さ35%
	ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.35f, nullptr, &center);

	// 残った中央を Inspector(左) / Preview(右) に分割
	ImGuiID preview = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.45f, nullptr, &center);

	ImGui::DockBuilderDockWindow("Effect Hierarchy", left);
	ImGui::DockBuilderDockWindow("Effect Inspector", center);	// 残った中央上
	ImGui::DockBuilderDockWindow("Effect Preview", preview);
	ImGui::DockBuilderDockWindow("Effect Assets", bottom);
	ImGui::DockBuilderDockWindow("Effect Editor", bottom);	// Effect Assetsとタブ化

	ImGui::DockBuilderFinish(dockspaceId);
}

void EffectEditor::Update()
{
	if (!ImGuizmo::IsUsing())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_1)) m_operation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_2)) m_operation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_3)) m_operation = ImGuizmo::SCALE;
	}

	CheckHotReload();

	const float deltaTime = Application::Instance().GetDeltaTime();

	for (auto& obj : m_objects)
	{
		if (obj.playing && !obj.paused)
		{
			UpdatePreview(obj, deltaTime);
		}
	}

	// エフェクトエディタ専用のコンテナウィンドウ
	//	メインビューポートの右外側に初期配置することで、
	//	マルチビューポート機能により起動時から「別ウィンドウ」として分離表示される
	ImGuiViewport* mainViewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos(
		ImVec2(mainViewport->Pos.x + mainViewport->Size.x + 20.0f, mainViewport->Pos.y),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(900.0f, 700.0f), ImGuiCond_FirstUseEver);

	ImGui::Begin("Effect Editor Window");
	{
		ImGuiID effectDockId = ImGui::GetID("EffectDockSpace");

		if (ImGui::DockBuilderGetNode(effectDockId) == nullptr)
		{
			SetupEffectDockLayout(effectDockId, ImGui::GetContentRegionAvail());
		}

		ImGui::DockSpace(effectDockId, ImVec2(0, 0));
	}
	ImGui::End();

	DrawMainMenu();
	DrawHierarchy();
	DrawInspector();
	DrawTexturePicker();
	DrawPreviewWindow();
	DrawGizmo();
}

// プレビュー中のGPUパーティクルを描画する
void EffectEditor::DrawPreviewParticles(ParticleDrawPass pass)
{
	for (auto& obj : m_objects)
	{
		// previewInstanceが未初期化(一度もPlayしていない)場合はDraw()側で何もしない
		if (obj.playing)
		{
			obj.previewInstance.Draw(pass);
		}
	}
}

// 選択中の1エフェクトを、専用カメラ・専用オフスクリーンバッファへ描画する
void EffectEditor::RenderPreviewViewport()
{
	// ウィンドウが一度も開かれておらずサイズが確定していない場合は何もしない
	if (!m_previewViewport.Color || !m_previewViewport.Depth || !m_previewViewport.Mask) return;
	if (m_previewViewport.Width <= 0 || m_previewViewport.Height <= 0) return;

	ID3D11DeviceContext* context = KdDirect3D::Instance().WorkDevContext();

	// 退避
	KdShaderManager::cbCamera savedCamera = KdShaderManager::Instance().GetCameraCB();

	ID3D11RenderTargetView* savedRTVs[2] = { nullptr, nullptr };
	ID3D11DepthStencilView* savedDSV = nullptr;
	context->OMGetRenderTargets(2, savedRTVs, &savedDSV);

	UINT savedVPNum = 1;
	D3D11_VIEWPORT savedVP = {};
	context->RSGetViewports(&savedVPNum, &savedVP);

	// プレビュー用バッファへ切り替え・クリア
	// ※スロット1(Mask)はカラーグレード処理を通らないこのプレビューでは中身を使わないが、
	//   Alphaブレンドのパーティクル(m_PS_Masked)がSV_Target1へ書き込めるように必要
	ID3D11RenderTargetView* rtvs[] = { m_previewViewport.Color->WorkRTView(), m_previewViewport.Mask->WorkRTView() };
	context->OMSetRenderTargets(2, rtvs, m_previewViewport.Depth->WorkDSView());

	static const float clearColor[4] = { 0.1f, 0.1f, 0.12f, 1.0f };
	context->ClearRenderTargetView(m_previewViewport.Color->WorkRTView(), clearColor);
	context->ClearRenderTargetView(m_previewViewport.Mask->WorkRTView(), kBlackColor);
	context->ClearDepthStencilView(m_previewViewport.Depth->WorkDSView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	D3D11_VIEWPORT vp = {};
	vp.Width = (float)m_previewViewport.Width;
	vp.Height = (float)m_previewViewport.Height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	context->RSSetViewports(1, &vp);

	if (m_selected >= 0 && m_selected < (int)m_objects.size())
	{
		EffectObject& obj = m_objects[m_selected];

		// プレビュー用カメラの適用
		DirectX::SimpleMath::Matrix view = m_previewCamera.GetView(obj.pos);
		DirectX::SimpleMath::Matrix proj = m_previewCamera.GetProj(
			(float)m_previewViewport.Width / (float)m_previewViewport.Height);

		KdShaderManager::Instance().WriteCBCamera(view.Invert(), proj);

		if (obj.playing)
		{
			obj.previewInstance.Draw();
		}
	}

	// 復元
	KdShaderManager::Instance().WriteCBCamera(savedCamera.mView.Invert(), savedCamera.mProj);

	context->OMSetRenderTargets(2, savedRTVs, savedDSV);
	if (savedRTVs[0]) { savedRTVs[0]->Release(); }
	if (savedRTVs[1]) { savedRTVs[1]->Release(); }
	if (savedDSV) { savedDSV->Release(); }

	context->RSSetViewports(savedVPNum, &savedVP);

	// 通常描画パイプライン(このプレビュー分の描画)が完全に終わった後、カラーグレードを適用する。
	// ※Apply()内部は自前のRT/ビューポート退避・復元を行うため、ここでの追加の後始末は不要
	m_previewPostProcess.Apply(m_previewViewport.Color);
}

void EffectEditor::DrawMainMenu()
{
	ImGui::Begin("Effect Editor");

	ImGui::InputText("Path", m_filePathBuf, sizeof(m_filePathBuf));

	if (ImGui::Button("Save")) { Save(m_filePathBuf); }
	ImGui::SameLine();
	if (ImGui::Button("Load")) { Load(m_filePathBuf); }
	ImGui::SameLine();
	ImGui::Checkbox("Auto Reload", &m_autoReload);

	ImGui::Separator();

	if (ImGui::Button("Play All")) { PlayAllPreview(); }
	ImGui::SameLine();
	if (ImGui::Button("Stop All")) { StopAllPreview(); }

	ImGui::Text("Effects : %d", (int)m_objects.size());

	ImGui::End();
}

void EffectEditor::DrawHierarchy()
{
	ImGui::Begin("Effect Hierarchy");

	if (ImGui::Button("+ Add"))
	{
		AddObject();
	}
	ImGui::SameLine();
	if (ImGui::Button("- Remove"))
	{
		RemoveSelected();
	}

	ImGui::Separator();

	for (int i = 0; i < (int)m_objects.size(); i++)
	{
		bool isSelected = (m_selected == i);

		std::string label = m_objects[i].name + "##" + std::to_string(i);
		if (ImGui::Selectable(label.c_str(), isSelected))
		{
			m_selected = i;
		}
	}

	ImGui::End();
}

void EffectEditor::DrawInspector()
{

	ImGui::Begin("Effect Inspector");

	if (m_selected < 0 || m_selected >= (int)m_objects.size())
	{
		ImGui::TextDisabled("エフェクトが選択されていません");
		ImGui::End();
		return;
	}

	EffectObject& obj = m_objects[m_selected];
	GPUParticleParams& params = obj.params;

	char nameBuf[128];
	strcpy_s(nameBuf, obj.name.c_str());
	if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
	{
		obj.name = nameBuf;
	}

	ImGui::Separator();
	ImGui::Text("Emission");

	{
		int capacity = (int)params.MaxParticleNum;
		if (ImGui::DragInt("Max Particle Num", &capacity, 1.0f, 1, 100000))
		{
			params.MaxParticleNum = (UINT)std::max(1, capacity);
		}
	}

	{
		const char* modeLabels[] = { "Burst", "Continuous" };
		int modeIdx = (params.EmitMode == ParticleEmitMode::Continuous) ? 1 : 0;
		if (ImGui::Combo("Emit Mode", &modeIdx, modeLabels, IM_ARRAYSIZE(modeLabels)))
		{
			params.EmitMode = (modeIdx == 1) ? ParticleEmitMode::Continuous : ParticleEmitMode::Burst;
		}
	}

	if (params.EmitMode == ParticleEmitMode::Burst)
	{
		ImGui::DragFloat("Emit Interval(sec)", &params.EmitInterval, 0.05f, 0.0f, 60.0f);
		ImGui::TextDisabled("Interval<=0 : 再生開始時に1回だけ発生。各LayerのCountは「1回あたりの発生数」");
	}
	else
	{
		ImGui::TextDisabled("各LayerのCountは「1秒あたりの発生数」として扱われます");
	}

	ImGui::DragFloat3("Gravity", &params.Gravity.x, 0.05f);

	ImGui::Separator();
	ImGui::Text("Material (WIP)");
	ImGui::Text("Texture : %s", params.TexturePath.empty() ? "(None)" : params.TexturePath.c_str());

	{
		const char* blendLabels[] = { "Add", "Alpha", "Multiply"};
		int blendIdx = static_cast<int>(params.BlendMode);

		if (ImGui::Combo("Blend Mode", &blendIdx, blendLabels, IM_ARRAYSIZE(blendLabels)))
		{
			params.BlendMode = static_cast<ParticleBlendMode>(blendIdx);
		}

		ImGui::TextDisabled("Alpha選択時、パーティクル同士の重なり順はソートされない(発生順のまま描画)");
	}

	{
		bool drawDefault = KdHasDrawPassFlag(params.DrawPassFlags, ParticleDrawPass::Default);
		bool drawBright = KdHasDrawPassFlag(params.DrawPassFlags, ParticleDrawPass::Bright);

		ImGui::Text("Draw Pass");
		bool changed = false;
		changed |= ImGui::Checkbox("Default", &drawDefault);
		ImGui::SameLine();
		changed |= ImGui::Checkbox("Bright", &drawBright);

		if (changed)
		{
			// 両方外すとどこにも描画されなくなってしまうので、最低Defaultだけは強制的に残す
			if (!drawDefault && !drawBright) { drawDefault = true; }

			ParticleDrawPass flags = static_cast<ParticleDrawPass>(0);
			if (drawDefault) { flags |= ParticleDrawPass::Default; }
			if (drawBright) { flags |= ParticleDrawPass::Bright; }
			params.DrawPassFlags = flags;
		}
		ImGui::TextDisabled("両方チェックすると、通常描画とブルーム(発光)の両方に同時に描画される");
	}

	ImGui::Separator();

	int deleteIndex = -1;

	for (int i = 0; i < (int)params.Layers.size(); i++)
	{
		if (DrawLayerInspector(params.Layers[i], i)) { deleteIndex = i; }
	}

	ImGui::Separator();

	ImGui::Text("Layers (%d)", (int)params.Layers.size());

	if (deleteIndex >= 0)
	{
		params.Layers.erase(params.Layers.begin() + deleteIndex);
	}

	if (ImGui::Button("+ Add Layer"))
	{
		params.Layers.push_back(GPUParticleLayer{});
	}

	ImGui::Separator();

	bool playing = obj.IsPlaying();
	if (!playing)
	{
		if (ImGui::Button("Play")) PlayPreview(obj);
	}
	else
	{
		if (ImGui::Button("Stop")) StopPreview(obj);
		ImGui::SameLine();

		if (!obj.paused)
		{
			if (ImGui::Button("Pause")) SetPreviewPause(obj, true);
		}
		else
		{
			if (ImGui::Button("Resume")) SetPreviewPause(obj, false);
		}
		ImGui::SameLine();

		if (ImGui::Button("Restart")) PlayPreview(obj);
	}

	ImGui::End();
}

bool EffectEditor::DrawLayerInspector(GPUParticleLayer& layer, int index)
{
	ImGui::PushID(index);
	DirectionalEmitShape& shape = layer.Shape;
	ParticleAppearance& app = layer.Appearance;

	std::string headerLabel = "Layer " + std::to_string(index);
	bool open = ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

	bool requestDelete = false;

	if (open)
	{
		ImGui::DragInt("Count", &layer.Count, 1.0f, 0, 100000);

		ImGui::SeparatorText("Shape");
		ImGui::DragFloat2("DirScale", &shape.DirScaleMin /* Min/Maxを並べる */);
		ImGui::DragFloat3("OffsetMin", &shape.OffsetMin.x);
		ImGui::DragFloat3("OffsetMax", &shape.OffsetMax.x);

		ImGui::SeparatorText("Appearance");
		ImGui::DragFloatRange2("Life Min/Max(sec)", &app.LifeMin, &app.LifeMax, 0.02f, 0.01f, 60.0f);
		ImGui::DragFloat2("Size Start", &app.SizeStartMin);
		ImGui::DragFloat2("Size End", &app.SizeEndMin);
		ImGui::SameLine();
		if (ImGui::SmallButton("Copy Start##Size"))
		{
			app.SizeEndMin = app.SizeStartMin;
			app.SizeEndMax = app.SizeStartMax;
		}

		ImGui::ColorEdit4("Color Start Min", &app.ColorStartMin.x);
		ImGui::ColorEdit4("Color Start Max", &app.ColorStartMax.x);
		ImGui::ColorEdit4("Color Min", &app.ColorMin.x);
		ImGui::ColorEdit4("Color Max", &app.ColorMax.x);
		ImGui::SameLine();
		if (ImGui::SmallButton("Copy Start##Color"))
		{
			app.ColorMin = app.ColorStartMin;
			app.ColorMax = app.ColorStartMax;
		}

		DrawColorRangeGradient(app, { 250,10 });

		ImGui::Separator();
		{
			const char* billboardLabels[] = { "Normal", "Stretch" };
			int billboardIdx = (layer.BillboardMode == ParticleBillboardMode::Stretch) ? 1 : 0;
			if (ImGui::Combo("Billboard Mode", &billboardIdx, billboardLabels, IM_ARRAYSIZE(billboardLabels)))
			{
				layer.BillboardMode = (billboardIdx == 1) ? ParticleBillboardMode::Stretch : ParticleBillboardMode::Normal;
			}

			if (layer.BillboardMode == ParticleBillboardMode::Stretch)
			{
				ImGui::DragFloat("Stretch Scale", &layer.StretchScale, 0.01f, 0.0f, 10.0f);
				ImGui::TextDisabled("速度が速いパーティクルほど進行方向へ伸びる(HitSpark/WeaponClash等の速い表現向け)");
			}
		}

		// 最後の1層は削除できないようにする
		if (ImGui::Button("- Remove This Layer"))
		{
			requestDelete = true;
		}
	}

	ImGui::PopID();

	return requestDelete;
}

void EffectEditor::DrawGizmo()
{
	if (m_selected < 0 || m_selected >= (int)m_objects.size()) return;

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();

	const ImVec2& rectPos = EditorViewport::Instance().GetScreenPos();
	const ImVec2& rectSize = EditorViewport::Instance().GetScreenSize();
	ImGuizmo::SetRect(rectPos.x, rectPos.y, rectSize.x, rectSize.y);

	const auto& cameraCB = KdShaderManager::Instance().GetCameraCB();
	const DirectX::SimpleMath::Matrix& view = cameraCB.mView;
	const DirectX::SimpleMath::Matrix& proj = cameraCB.mProj;

	EffectObject& obj = m_objects[m_selected];

	float matrix[16];
	ImGuizmo::RecomposeMatrixFromComponents(&obj.pos.x, &obj.rotate.x, &obj.scale.x, matrix);

	ImGuizmo::Manipulate(
		reinterpret_cast<const float*>(&view),
		reinterpret_cast<const float*>(&proj),
		m_operation, m_mode, matrix,
		nullptr,
		m_useSnap ? m_snapValue : nullptr);

	if (ImGuizmo::IsUsing())
	{
		ImGuizmo::DecomposeMatrixToComponents(matrix, &obj.pos.x, &obj.rotate.x, &obj.scale.x);
	}
}

void EffectEditor::DrawTexturePicker()
{
	ImGui::Begin("Effect Assets");

	if (!m_textureListLoaded)
	{
		RefreshTextureFileList();
		m_textureListLoaded = true;
	}

	if (ImGui::Button("Refresh"))
	{
		RefreshTextureFileList();
	}
	ImGui::SameLine();
	ImGui::Checkbox("Auto Preview", &m_autoPreviewOnSelect);

	ImGui::Separator();

	if (m_selected < 0 || m_selected >= (int)m_objects.size())
	{
		ImGui::TextDisabled("エフェクトオブジェクトを選択してください");
		ImGui::End();
		return;
	}

	EffectObject& obj = m_objects[m_selected];

	ImGui::Text("Current : %s", obj.params.TexturePath.empty() ? "(None)" : obj.params.TexturePath.c_str());
	ImGui::Separator();

	for (auto& path : m_textureFileList)
	{
		bool isSelected = (obj.params.TexturePath == path);
		if (ImGui::Selectable(path.c_str(), isSelected))
		{
			obj.params.TexturePath = path;

			// Auto Preview有効時は選択した瞬間にその場で再生し、見た目をすぐ確認できるようにする
			// (previewInstance側のテクスチャ再解決はPlayPreview()内のReconfigure()が行う)
			if (m_autoPreviewOnSelect)
			{
				PlayPreview(obj);
			}
			else if (obj.playing)
			{
				// 再生中に差し替えた場合はその場でテクスチャだけ読み直す
				obj.previewInstance.Reconfigure(obj.params, &m_textureProvider);
			}
		}
	}

	ImGui::End();
}

void EffectEditor::DrawPreviewWindow()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("Effect Preview");

	ImVec2 regionSize = ImGui::GetContentRegionAvail();

	// ウィンドウサイズが変わったらオフスクリーンバッファを作り直す
	if (regionSize.x >= 1.0f && regionSize.y >= 1.0f)
	{
		m_previewViewport.Resize((int)regionSize.x, (int)regionSize.y);
	}

	if (m_previewViewport.Color)
	{
		m_previewViewport.ScreenPos = ImGui::GetCursorScreenPos();
		m_previewViewport.ScreenSize = regionSize;

		// カラーグレード適用後の結果を表示する。リサイズ直後で結果がまだ無い場合のみ、
		// 生のプレビュー画像にフォールバックする(次フレームには結果が揃う)
		const std::shared_ptr<KdTexture>& displayTex =
			m_previewPostProcess.GetResultTexture() ? m_previewPostProcess.GetResultTexture() : m_previewViewport.Color;

		ImGui::Image((ImTextureID)displayTex->WorkSRView(), regionSize);

		// 右ドラッグ：オービット回転、ホイール：ズーム
		if (ImGui::IsItemHovered())
		{
			ImGuiIO& io = ImGui::GetIO();

			if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
			{
				m_previewCamera.Yaw -= io.MouseDelta.x * 0.01f;
				m_previewCamera.Pitch += io.MouseDelta.y * 0.01f;
				m_previewCamera.Pitch = std::clamp(m_previewCamera.Pitch, -1.5f, 1.5f);
			}

			if (io.MouseWheel != 0.0f)
			{
				m_previewCamera.Distance -= io.MouseWheel * 0.3f;
				m_previewCamera.Distance = std::clamp(m_previewCamera.Distance, 0.2f, 50.0f);
			}
		}
	}

	if (m_selected < 0 || m_selected >= (int)m_objects.size())
	{
		ImGui::SetCursorPos(ImVec2(10, 10));
		ImGui::TextDisabled("エフェクトが選択されていません");
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

// x軸：寿命の進行(左=Start / 右=End)
// y軸：Min-Maxの乱数幅(上=Max側 / 下=Min側)
void EffectEditor::DrawColorRangeGradient(const ParticleAppearance& a, ImVec2 size)
{
	ImVec2 p0 = ImGui::GetCursorScreenPos();
	ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);

	ImU32 topLeft = ImGui::ColorConvertFloat4ToU32(ToImVec4(a.ColorStartMax)); // Start側Max
	ImU32 topRight = ImGui::ColorConvertFloat4ToU32(ToImVec4(a.ColorMax));      // End側Max
	ImU32 botRight = ImGui::ColorConvertFloat4ToU32(ToImVec4(a.ColorMin));      // End側Min
	ImU32 botLeft = ImGui::ColorConvertFloat4ToU32(ToImVec4(a.ColorStartMin)); // Start側Min

	ImGui::GetWindowDrawList()->AddRectFilledMultiColor(p0, p1, topLeft, topRight, botRight, botLeft);
	ImGui::Dummy(size); // レイアウト上の場所取り
}

void EffectEditor::PreviewViewport::Resize(int w, int h)
{
	if (w <= 0 || h <= 0) return;

	// サイズが変わっていなければ作り直さない
	if (w == Width && h == Height && Color && Depth && Mask) return;

	Width = w;
	Height = h;

	// ----- カラーバッファ -----
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

		Color = std::make_shared<KdTexture>();
		Color->Create(desc);
	}

	// ----- Zバッファ -----
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

		Depth = std::make_shared<KdTexture>();
		Depth->Create(desc);
	}

	// ----- カラーグレード除外マスク書き込み用の捨てRT -----
	//	このプレビューはカラーグレード処理を通らないため中身は使わないが、
	//	Alphaブレンドのパーティクル(m_PS_Masked)がSV_Target1へ書き込めるように
	//	スロット1として何かバインドしておく必要がある(単チャンネルで十分)
	{
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.Format = DXGI_FORMAT_R8_UNORM;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET;
		desc.Width = (UINT)w;
		desc.Height = (UINT)h;
		desc.CPUAccessFlags = 0;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		Mask = std::make_shared<KdTexture>();
		Mask->Create(desc);
	}
}

DirectX::SimpleMath::Matrix EffectEditor::PreviewCamera::GetView(const DirectX::SimpleMath::Vector3& target) const
{
	using namespace DirectX::SimpleMath;

	float cosPitch = cosf(Pitch);
	Vector3 offset(
		Distance * cosPitch * sinf(Yaw),
		Distance * sinf(Pitch),
		Distance * cosPitch * cosf(Yaw));

	Vector3 eye = target + offset;
	return Matrix::CreateLookAt(eye, target, Vector3::Up);
}

DirectX::SimpleMath::Matrix EffectEditor::PreviewCamera::GetProj(float aspect) const
{
	return DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(
		DirectX::XMConvertToRadians(45.0f), aspect, 0.05f, 100.0f);
}

void EffectEditor::RefreshTextureFileList()
{
	m_textureFileList.clear();

	namespace fs = std::filesystem;

	const std::string root = kTextureAssetRoot;

	if (!fs::exists(root)) return;

	static const std::vector<std::string> kExtensions = { ".png", ".dds", ".jpg", ".jpeg", ".tga" };

	for (auto& entry : fs::recursive_directory_iterator(root))
	{
		if (!entry.is_regular_file()) continue;

		std::string ext = entry.path().extension().string();
		for (auto& c : ext) c = (char)tolower(c);

		if (std::find(kExtensions.begin(), kExtensions.end(), ext) == kExtensions.end()) continue;

		std::string relativePath = fs::relative(entry.path(), root).generic_string();
		m_textureFileList.push_back(relativePath);
	}
}

EffectObject* EffectEditor::FindObjectByName(const std::string& name)
{
	for (auto& obj : m_objects)
	{
		if (obj.name == name) { return &obj; }
	}
	return nullptr;
}

EffectEditor::EffectEditor()
{
	Load(m_filePathBuf);
}

void EffectEditor::PlayPreview(EffectObject& obj)
{
	obj.previewInstance.Reconfigure(obj.params, &m_textureProvider);

	obj.playing = true;
	obj.paused = false;

	obj.previewInstance.Play(obj.pos,{1,1,1});
}

void EffectEditor::StopPreview(EffectObject& obj)
{
	obj.playing = false;
	obj.paused = false;

	obj.previewInstance.Stop();
}

void EffectEditor::SetPreviewPause(EffectObject& obj, bool pause)
{
	if (!obj.playing) return;
	obj.paused = pause;
}

void EffectEditor::UpdatePreview(EffectObject& obj, float deltaTime)
{
	obj.previewInstance.Reconfigure(obj.params, &m_textureProvider);

	obj.previewInstance.Update(deltaTime, obj.pos);
}

void EffectEditor::PlayAllLooping()
{
	for (auto& obj : m_objects)
	{
		if (obj.params.IsLooping() && !obj.IsPlaying())
		{
			PlayPreview(obj);
		}
	}
}

void EffectEditor::PlayAllPreview()
{
	for (auto& obj : m_objects)
	{
		PlayPreview(obj);
	}
}

void EffectEditor::StopAllPreview()
{
	for (auto& obj : m_objects)
	{
		StopPreview(obj);
	}
}

void EffectEditor::AddObject()
{
	EffectObject obj;
	obj.name = "Effect" + std::to_string(m_objects.size());
	m_objects.push_back(std::move(obj));	// EffectObjectはEffectInstanceを持つ為コピー不可、moveする
	m_selected = (int)m_objects.size() - 1;
}

void EffectEditor::RemoveSelected()
{
	if (m_selected < 0 || m_selected >= (int)m_objects.size()) return;

	StopPreview(m_objects[m_selected]);
	m_objects.erase(m_objects.begin() + m_selected);
	m_selected = -1;
}

void EffectEditor::Save(const std::string& path)
{
	EffectDataFile data;
	data.Effects.reserve(m_objects.size());

	for (auto& obj : m_objects)
	{
		EffectDefinition def;
		def.Name = obj.name;
		def.Pos = obj.pos;
		def.Rotate = obj.rotate;
		def.Scale = obj.scale;
		def.Params = obj.params;
		data.Effects.push_back(def);
	}

	if (!EffectDataLoader::Save(path, data))
	{
		KdDebugGUI::Instance().AddLog("EffectEditor: 保存に失敗 %s\n", path.c_str());
		return;
	}

	// 実行中で、同じJSONを読み込んでいるEffectDispatcherがあれば、その場で再ロードさせて
	// エディタでの変更を即座にゲーム側(実行中のプレイ)へ反映する
	EffectDispatcher::NotifyDataSaved(path);

	FILETIME writeTime;
	if (JsonLoader::GetLastWriteTime(path, writeTime))
	{
		m_lastWriteTime = writeTime;
	}

	KdDebugGUI::Instance().AddLog("EffectEditor: 保存しました %s\n", path.c_str());
}

void EffectEditor::Load(const std::string& path)
{
	EffectDataFile data;
	if (!EffectDataLoader::Load(path, data))
	{
		KdDebugGUI::Instance().AddLog("EffectEditor: 読み込み失敗 %s\n", path.c_str());
		return;
	}

	// 読み込み前に、現在再生中のプレビューを全て止めておく
	for (auto& obj : m_objects)
	{
		StopPreview(obj);
	}
	m_objects.clear();

	for (auto& def : data.Effects)
	{
		EffectObject obj;
		obj.name = def.Name;
		obj.pos = def.Pos;
		obj.rotate = def.Rotate;
		obj.scale = def.Scale;
		obj.params = def.Params;
		m_objects.push_back(std::move(obj));
	}

	m_selected = -1;
	KdDebugGUI::Instance().AddLog("EffectEditor: 読み込みました %s\n", path.c_str());
}

void EffectEditor::CheckHotReload()
{
	if (!m_autoReload) return;

	m_reloadCheckTimer += Application::Instance().GetDeltaTime();
	if (m_reloadCheckTimer < 0.5f) return;
	m_reloadCheckTimer = 0.0f;

	FILETIME writeTime;
	if (!JsonLoader::GetLastWriteTime(m_filePathBuf, writeTime))
	{
		return;
	}

	if (m_lastWriteTime.dwLowDateTime == 0 && m_lastWriteTime.dwHighDateTime == 0)
	{
		m_lastWriteTime = writeTime;
		return;
	}

	if (CompareFileTime(&writeTime, &m_lastWriteTime) == 0)
	{
		return;
	}

	m_lastWriteTime = writeTime;

	int keepSelected = m_selected;

	Load(m_filePathBuf);

	if (keepSelected >= 0 && keepSelected < (int)m_objects.size())
	{
		m_selected = keepSelected;
	}

	KdDebugGUI::Instance().AddLog("EffectEditor: 外部変更を検知し自動リロードしました (%s)\n", m_filePathBuf);
}