#include "Application/main.h"

#include "EffectEditor.h"
#include "../Common/EditorViewport.h"
#include "Application/Definitions/Loaders/EffectDataLoader.h"
#include "../../Effect/Particle/EffectDispatcher.h"

#include "imgui_internal.h"

#include <filesystem>

static const std::string kTextureAssetRoot = "Asset/Textures/Game/Effect/";

// 再生中の液体エフェクトを、密度RTへまとめて描いて合成する(液体が無ければ何もしない)
//	useSceneDepth：メインシーンへ合成するならtrue、プレビュー専用RTへ描くならfalse
static void DrawLiquidObjects(const std::vector<EffectObject*>& targets, bool useSceneDepth)
{
	bool hasLiquid = false;
	for (const EffectObject* obj : targets) { hasLiquid |= obj->playing && obj->previewInstance.IsLiquid(); }
	if (!hasLiquid) { return; }

	KdPostProcessShader& postProcess = KdShaderManager::Instance().m_postProcessShader;

	postProcess.BeginLiquid(useSceneDepth);
	for (const EffectObject* obj : targets) { if (obj->playing) { obj->previewInstance.DrawLiquid(); } }
	postProcess.EndLiquid(LiquidStyle::Ink);
}

// 墨(LiquidInk)の質感パラメータ。全エフェクト共通の値で、JSONには保存されない
static void DrawLiquidInkSettings()
{
	auto& cb = KdShaderManager::Instance().m_postProcessShader.WorkLiquidCB();

	ImGui::Indent();
	ImGui::TextDisabled("Liquid Ink(全エフェクト共通。Saveでeffectmap.jsonの\"liquidInk\"へ保存)");
	ImGui::SliderFloat("Threshold", &cb.Threshold, 0.0f, 1.0f);
	ImGui::SliderFloat("Softness", &cb.Softness, 0.005f, 0.3f);
	ImGui::SliderFloat("Edge Width", &cb.EdgeWidth, 0.01f, 0.5f);
	ImGui::SliderFloat("Halo Alpha", &cb.HaloAlpha, 0.0f, 1.0f);
	ImGui::ColorEdit3("Ink Color", &cb.InkColor.x);
	ImGui::ColorEdit3("Edge Color", &cb.EdgeColor.x);

	// 調整した値をコードへ貼り付けられる形でコピーする(シーン初期化時に設定する用)
	if (ImGui::Button("Copy as Code"))
	{
		char buf[768];
		snprintf(buf, sizeof(buf),
			"auto& liquid = KdShaderManager::Instance().m_postProcessShader.WorkLiquidCB();\n"
			"liquid.Threshold = %.3ff;\nliquid.Softness = %.3ff;\nliquid.EdgeWidth = %.3ff;\nliquid.HaloAlpha = %.3ff;\n"
			"liquid.InkColor = { %.3ff, %.3ff, %.3ff };\nliquid.EdgeColor = { %.3ff, %.3ff, %.3ff };\n",
			cb.Threshold, cb.Softness, cb.EdgeWidth, cb.HaloAlpha,
			cb.InkColor.x, cb.InkColor.y, cb.InkColor.z, cb.EdgeColor.x, cb.EdgeColor.y, cb.EdgeColor.z);
		ImGui::SetClipboardText(buf);
	}

	ImGui::TextDisabled("粒のColorは密度として使う：ColorStart rgbを白寄り、Color rgbを黒にすると、寿命で縮んで消える");
	ImGui::Unindent();
}

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
	ImGui::DockBuilderDockWindow("Effect Groups", left);	// Effect Hierarchyとタブ化
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
	DrawGroupsPanel();
	DrawInspector();
	DrawTexturePicker();
	DrawPreviewWindow();
	DrawGizmo();
}

// プレビュー中のGPUパーティクルを描画する
void EffectEditor::DrawPreviewParticles(ParticleDrawPass pass)
{
	// 液体はDraw(pass)では描画されないため、Defaultのタイミングで他の粒子より先にまとめて描く
	if (pass == ParticleDrawPass::Default)
	{
		std::vector<EffectObject*> targets;
		for (auto& obj : m_objects) { targets.push_back(&obj); }
		DrawLiquidObjects(targets, true);
	}

	for (auto& obj : m_objects)
	{
		// previewInstanceが未初期化(一度もPlayしていない)場合はDraw()側で何もしない
		if (obj.playing)
		{
			obj.previewInstance.Draw(pass);
		}
	}
}

// 選択中の1エフェクト(またはプレビュー中グループの全メンバー)を、専用カメラ・専用バッファへ描画する
void EffectEditor::RenderPreviewViewport()
{
	using namespace DirectX::SimpleMath;

	// ウィンドウが一度も開かれておらずサイズが確定していない場合は何もしない
	if (!m_preview.IsReady()) return;

	// 描画対象と注視点を決める。
	// グループプレビュー中：実在するメンバー全員、注視点はそのposの平均値
	// 単体選択中：選択中の1つ、注視点はそのpos。どちらも無ければ原点を見るだけで何も描かない
	Vector3 target = { 0,0,0 };
	std::vector<EffectObject*> drawTargets;

	if (!m_previewedGroup.empty())
	{
		auto groupIt = m_groups.find(m_previewedGroup);
		if (groupIt != m_groups.end())
		{
			Vector3 sum = { 0,0,0 };
			for (const auto& memberName : groupIt->second)
			{
				if (EffectObject* obj = FindObjectByName(memberName))
				{
					sum += obj->pos;
					drawTargets.push_back(obj);
				}
			}
			if (!drawTargets.empty()) { target = sum / (float)drawTargets.size(); }
		}
	}
	else if (m_selected >= 0 && m_selected < (int)m_objects.size())
	{
		EffectObject& obj = m_objects[m_selected];
		target = obj.pos;
		drawTargets.push_back(&obj);
	}

	// 本編のDrawLit/DrawBloomと同じく、Default(通常合成)とBright(Bloom用に加算描画)を
	// 呼び分ける。EffectInstance::Draw()(無条件版)ではなくDraw(pass)を使うのがポイント
	m_preview.Render(target,
		[&]()
		{
			DrawLiquidObjects(drawTargets, false);

			for (EffectObject* obj : drawTargets)
			{
				if (obj->playing) { obj->previewInstance.Draw(ParticleDrawPass::Default); }
			}
		},
		[&]()
		{
			for (EffectObject* obj : drawTargets)
			{
				if (obj->playing) { obj->previewInstance.Draw(ParticleDrawPass::Bright); }
			}
		});
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
	ImGui::SameLine();
	if (ImGui::Button("Copy"))
	{
		CopySelected();
	}

	ImGui::Separator();

	for (int i = 0; i < (int)m_objects.size(); i++)
	{
		bool isSelected = (m_selected == i);

		std::string label = m_objects[i].name + "##" + std::to_string(i);
		if (ImGui::Selectable(label.c_str(), isSelected))
		{
			m_selected = i;
			m_previewedGroup.clear();	// 単体選択に戻った時はグループプレビューを解除する
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
		const char* blendLabels[] = { "Add", "Alpha", "Multiply", "LiquidInk" };
		int blendIdx = static_cast<int>(params.BlendMode);

		if (ImGui::Combo("Blend Mode", &blendIdx, blendLabels, IM_ARRAYSIZE(blendLabels)))
		{
			params.BlendMode = static_cast<ParticleBlendMode>(blendIdx);
		}

		ImGui::TextDisabled("Alpha選択時、パーティクル同士の重なり順はソートされない(発生順のまま描画)");
	}

	const bool isLiquid = (params.BlendMode == ParticleBlendMode::LiquidInk);

	if (isLiquid)
	{
		DrawLiquidInkSettings();
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
		if (isLiquid) { ImGui::TextDisabled("LiquidInk時はこの設定を無視し、Defaultのタイミングで描画される"); }
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
		{
			const char* distributionLabels[] = { "Directional", "Radial In Plane" };
			int distributionIdx = (shape.Distribution == ParticleEmitDistribution::RadialInPlane) ? 1 : 0;
			if (ImGui::Combo("Distribution", &distributionIdx, distributionLabels, IM_ARRAYSIZE(distributionLabels)))
			{
				shape.Distribution = (distributionIdx == 1) ? ParticleEmitDistribution::RadialInPlane : ParticleEmitDistribution::Directional;
			}

			if (shape.Distribution == ParticleEmitDistribution::RadialInPlane)
			{
				ImGui::DragFloat2("Radial Speed", &shape.RadialSpeedMin /* Min/Maxを並べる */);
				ImGui::TextDisabled("水平面内のランダムな角度へ均等に飛ばす(角度はGPU側で毎粒子ごとに決定)");
			}
			else
			{
				ImGui::DragFloat2("DirScale", &shape.DirScaleMin /* Min/Maxを並べる */);
				ImGui::DragFloat3("OffsetMin", &shape.OffsetMin.x);
				ImGui::DragFloat3("OffsetMax", &shape.OffsetMax.x);
			}
		}

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
			const char* billboardLabels[] = { "Normal", "Stretch", "Beam" };
			int billboardIdx = 0;
			if (layer.BillboardMode == ParticleBillboardMode::Stretch) { billboardIdx = 1; }
			else if (layer.BillboardMode == ParticleBillboardMode::Beam) { billboardIdx = 2; }
			if (ImGui::Combo("Billboard Mode", &billboardIdx, billboardLabels, IM_ARRAYSIZE(billboardLabels)))
			{
				if (billboardIdx == 1) { layer.BillboardMode = ParticleBillboardMode::Stretch; }
				else if (billboardIdx == 2) { layer.BillboardMode = ParticleBillboardMode::Beam; }
				else { layer.BillboardMode = ParticleBillboardMode::Normal; }
			}

			if (layer.BillboardMode == ParticleBillboardMode::Stretch)
			{
				ImGui::DragFloat("Stretch Scale", &layer.StretchScale, 0.01f, 0.0f, 10.0f);
				ImGui::TextDisabled("速度が速いパーティクルほど進行方向へ伸びる(HitSpark/WeaponClash等の速い表現向け)");
			}
			else if (layer.BillboardMode == ParticleBillboardMode::Beam)
			{
				ImGui::DragFloat("Beam Width", &layer.StretchScale, 0.005f, 0.0f, 5.0f);
				ImGui::TextDisabled("中心(発生点)から両端に伸びる線。Shapeで速度を0にし、Size Start/Endで長さを制御する想定");
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
	m_preview.DrawWindow("Effect Preview", 0, [this]()
		{
			if (!m_previewedGroup.empty())
			{
				ImGui::SetCursorPos(ImVec2(10, 10));
				ImGui::Text("Group : %s", m_previewedGroup.c_str());
			}
			else if (m_selected < 0 || m_selected >= (int)m_objects.size())
			{
				ImGui::SetCursorPos(ImVec2(10, 10));
				ImGui::TextDisabled("エフェクトが選択されていません");
			}
		});
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

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// グループ(複数エフェクトをまとめて1つの名前で発生させる為の定義)の管理・プレビュー用パネル
//	m_groups自体はここでしか編集しない。メンバー名の実在チェックは行わず、
//	m_objectsに実在しないメンバーは一覧で赤字表示するだけに留める
//	(EffectDispatcher側の「実在確認はロード時、Emit解決はfindするだけ」という方針と揃えている)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectEditor::DrawGroupsPanel()
{
	ImGui::Begin("Effect Groups");

	static char newGroupNameBuf[128] = "";
	ImGui::InputText("New Group Name", newGroupNameBuf, sizeof(newGroupNameBuf));
	ImGui::SameLine();
	if (ImGui::Button("+ Add Group"))
	{
		std::string groupName = newGroupNameBuf;
		if (!groupName.empty() && m_groups.find(groupName) == m_groups.end())
		{
			m_groups[groupName] = {};
			newGroupNameBuf[0] = '\0';
		}
	}

	ImGui::Separator();

	// ループ中にm_groups自体へキーの追加/削除を行うと反復子が壊れるので、
	// 削除要求はここに溜めてループを抜けてから実行する(DrawLayerInspectorと同じ方針)
	std::string groupToRemove;

	for (auto& pair : m_groups)
	{
		const std::string& groupName = pair.first;
		std::vector<std::string>& members = pair.second;

		ImGui::PushID(groupName.c_str());

		// AllowOverlapを付けないと、この後SameLineで重ねるボタンがヘッダにクリックを奪われて反応しない
		bool open = ImGui::CollapsingHeader(groupName.c_str(), ImGuiTreeNodeFlags_AllowOverlap);

		ImGui::SameLine();
		if (ImGui::SmallButton("Play"))
		{
			// メンバーのうちm_objectsに実在するものだけ、それぞれの単体Play相当で再生する
			for (const auto& memberName : members)
			{
				if (EffectObject* obj = FindObjectByName(memberName))
				{
					PlayPreview(*obj);
				}
			}
			// Effect Previewウィンドウをこのグループ全員表示に切り替える(単体選択は解除)
			m_previewedGroup = groupName;
			m_selected = -1;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Stop"))
		{
			for (const auto& memberName : members)
			{
				if (EffectObject* obj = FindObjectByName(memberName))
				{
					StopPreview(*obj);
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Delete Group"))
		{
			groupToRemove = groupName;
		}

		if (open)
		{
			int removeMemberIndex = -1;

			for (int i = 0; i < (int)members.size(); i++)
			{
				ImGui::PushID(i);

				const bool exists = (FindObjectByName(members[i]) != nullptr);
				if (exists)
				{
					ImGui::Text("%s", members[i].c_str());
				}
				else
				{
					// m_objectsから消えた/リネームされたメンバー。保存はするが赤字で気付けるようにする
					ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s (見つかりません)", members[i].c_str());
				}

				ImGui::SameLine();
				if (ImGui::SmallButton("x")) { removeMemberIndex = i; }

				ImGui::PopID();
			}

			if (removeMemberIndex >= 0)
			{
				members.erase(members.begin() + removeMemberIndex);
			}

			// 既存のm_objectsから、まだこのグループに入っていないものだけを選択肢として出す
			if (ImGui::BeginCombo("+ Add Member", "選択..."))
			{
				for (auto& obj : m_objects)
				{
					bool alreadyIn = std::find(members.begin(), members.end(), obj.name) != members.end();
					if (alreadyIn) { continue; }

					if (ImGui::Selectable(obj.name.c_str()))
					{
						members.push_back(obj.name);
					}
				}
				ImGui::EndCombo();
			}
		}

		ImGui::PopID();
	}

	if (!groupToRemove.empty())
	{
		m_groups.erase(groupToRemove);
	}

	ImGui::End();
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
	// パーティクルのAlphaブレンド用にMask RTが必要。カメラ設定はエフェクト単体の確認向け(近距離・狭い範囲)
	EditorPreviewViewport::Settings previewSettings;
	previewSettings.UseMaskRT = true;
	previewSettings.UseBrightRT = true;	// ParticleDrawPass::Bright(Bloom用)のプレビューに必要
	previewSettings.FarZ = 100.0f;
	previewSettings.InitialDistance = 3.0f;
	previewSettings.MaxDistance = 50.0f;
	previewSettings.WheelSensitivity = 0.3f;
	m_preview.Configure(previewSettings);

	Load(m_filePathBuf);
}

void EffectEditor::PlayPreview(EffectObject& obj)
{
	obj.previewInstance.Reconfigure(obj.params, &m_textureProvider);

	obj.playing = true;
	obj.paused = false;

	obj.previewInstance.Play(obj.pos, { 1,1,1 });
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

void EffectEditor::CopySelected()
{
	if (m_selected < 0 || m_selected >= (int)m_objects.size()) return;
	EffectObject obj;
	obj.name = m_objects[m_selected].name + std::string("_copy");
	obj.params = m_objects[m_selected].params;
	m_objects.push_back(std::move(obj));
	m_selected = (int)m_objects.size() - 1;
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

	data.Groups = m_groups;

	// 墨の質感(インスペクターで調整した全体設定)も一緒に保存する
	data.LiquidInk = EffectDataLoader::CaptureLiquidInk();

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

	m_groups = data.Groups;

	if (data.LiquidInk) { EffectDataLoader::ApplyLiquidInk(*data.LiquidInk); }

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