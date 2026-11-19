#include "Application/main.h"

#include "SlashTrailEditor.h"
#include "../Common/EditorUtil.h"
#include "Application/Definitions/Loaders/SlashTrailDataLoader.h"

static const std::string kTextureAssetRoot = "Asset/Textures/Game/Effect/";

namespace
{
	bool DragUInt(const char* label, UINT& value, float speed, int minV, int maxV)
	{
		int v = static_cast<int>(value);
		if (!ImGui::DragInt(label, &v, speed, minV, maxV)) { return false; }

		value = static_cast<UINT>(v);
		return true;
	}

	// Add / Alpha / Multiply(トレイルではLiquidInkは対象外)
	bool BlendModeCombo(const char* label, ParticleBlendMode& mode)
	{
		const char* labels[] = { "Add", "Alpha", "Multiply" };
		int idx = std::min(static_cast<int>(mode), 2);

		if (!ImGui::Combo(label, &idx, labels, IM_ARRAYSIZE(labels))) { return false; }

		mode = static_cast<ParticleBlendMode>(idx);
		return true;
	}

	// 両方外すとどこにも描かれないので、最低Defaultは残す
	bool DrawPassEdit(ParticleDrawPass& flags)
	{
		bool def = KdHasDrawPassFlag(flags, ParticleDrawPass::Default);
		bool bright = KdHasDrawPassFlag(flags, ParticleDrawPass::Bright);

		bool changed = ImGui::Checkbox("Default", &def);
		ImGui::SameLine();
		changed |= ImGui::Checkbox("Bright", &bright);

		if (changed)
		{
			if (!def && !bright) { def = true; }

			ParticleDrawPass result = static_cast<ParticleDrawPass>(0);
			if (def) { result |= ParticleDrawPass::Default; }
			if (bright) { result |= ParticleDrawPass::Bright; }
			flags = result;
		}
		return changed;
	}
}

SlashTrailEditor::SlashTrailEditor()
{
	// Alphaブレンド用にMask RT、Bright描画用にBright RTが必要
	EditorPreviewViewport::Settings settings;
	settings.UseMaskRT = true;
	settings.UseBrightRT = true;
	settings.InitialDistance = 4.0f;
	settings.MaxDistance = 50.0f;
	m_preview.Configure(settings);

	Load(m_filePathBuf);
}

void SlashTrailEditor::Update()
{
	CheckHotReload();
	UpdatePlayback(Application::Instance().GetDeltaTime());

	DrawMainMenu();
	DrawHierarchy();
	DrawInspector();
	DrawPreviewWindow();
}

SlashTrailObject* SlashTrailEditor::Selected()
{
	if (m_selected < 0 || m_selected >= static_cast<int>(m_objects.size())) { return nullptr; }
	return &m_objects[m_selected];
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// プレビュー描画
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailEditor::RenderPreviewViewport()
{
	if (!m_preview.IsReady()) { return; }

	const float deltaTime = Application::Instance().GetDeltaTime();

	// 頂点はカメラ正対で作られるので、Update()もプレビューカメラが有効なこの中で行う
	m_preview.Render({ 0.0f, 0.8f, 0.0f },
		[&]()
		{
			m_dispatcher.Update(deltaTime);
			m_dispatcher.Draw(ParticleDrawPass::Default);
		},
		[&]() { m_dispatcher.Draw(ParticleDrawPass::Bright); });
}

void SlashTrailEditor::DrawPreviewWindow()
{
	m_preview.DrawWindow("SlashTrail Preview", 0, [this]()
		{
			if (!Selected())
			{
				ImGui::SetCursorPos(ImVec2(10, 10));
				ImGui::TextDisabled("トレイルが選択されていません");
			}
		});
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// プレビュー再生
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailEditor::Play()
{
	SlashTrailObject* obj = Selected();
	if (!obj) { return; }

	Stop();

	// 編集があった時だけ登録し直す(登録のたびにスロットの頂点バッファが作り直されるため)
	if (m_defStale || m_defId == kInvalidSlashTrailDef)
	{
		m_defId = m_dispatcher.RegisterDefinition(obj->name, obj->params);
		m_defStale = false;
	}

	m_handle = m_dispatcher.Acquire(m_defId);
	if (!m_handle.IsValid()) { return; }

	m_recorder.Init(obj->params);
	m_recorder.Begin();

	m_fadeWait = obj->params.Dissolve.Time + 0.3f;
	m_time = 0.0f;
	m_state = PlayState::Swinging;
}

// 振り途中なら記録を確定してフェードアウトへ移す
void SlashTrailEditor::Stop()
{
	if (m_state == PlayState::Swinging)
	{
		m_recorder.End();
		m_dispatcher.Finish(m_handle, m_recorder);
	}
	m_state = PlayState::Idle;
}

void SlashTrailEditor::UpdatePlayback(float deltaTime)
{
	using namespace DirectX::SimpleMath;

	if (m_state == PlayState::Idle) { return; }

	m_time += deltaTime;

	if (m_state == PlayState::Swinging)
	{
		const float t = std::clamp(m_time / std::max(m_swing.Duration, 0.01f), 0.0f, 1.0f);
		const float angle = DirectX::XMConvertToRadians(std::lerp(m_swing.ArcStartDeg, m_swing.ArcEndDeg, t));
		const float tilt = DirectX::XMConvertToRadians(m_swing.PlaneTiltDeg);

		const Vector3 dir(cosf(angle), sinf(angle) * cosf(tilt), sinf(angle) * sinf(tilt));

		// 記録の呼び順は UpdateTipBase → Tick(SlashTrailRecorder.hの使い方通り)
		m_recorder.UpdateTipBase(dir * (m_swing.Radius + m_swing.SwordLength), dir * m_swing.Radius);
		m_recorder.Tick(deltaTime);
		m_dispatcher.SubmitLive(m_handle, m_recorder);

		if (t >= 1.0f)
		{
			m_recorder.End();
			m_dispatcher.Finish(m_handle, m_recorder);
			m_state = PlayState::Fading;
			m_time = 0.0f;
		}
	}
	else if (m_time >= m_fadeWait)
	{
		m_state = PlayState::Idle;
		if (m_loop) { Play(); }
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ウィンドウ
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailEditor::DrawMainMenu()
{
	ImGui::Begin("SlashTrail Editor");

	ImGui::InputText("Path", m_filePathBuf, sizeof(m_filePathBuf));

	if (ImGui::Button("Save")) { Save(m_filePathBuf); }
	ImGui::SameLine();
	if (ImGui::Button("Load")) { Load(m_filePathBuf); }
	ImGui::SameLine();
	ImGui::Checkbox("Auto Reload", &m_watcher.autoReload);

	ImGui::Separator();

	if (ImGui::Button("Play")) { Play(); }
	ImGui::SameLine();
	if (ImGui::Button("Stop")) { Stop(); }
	ImGui::SameLine();
	ImGui::Checkbox("Loop", &m_loop);

	ImGui::Text("Swing(プレビュー用。保存されない)");
	ImGui::DragFloat("Duration(sec)", &m_swing.Duration, 0.01f, 0.05f, 5.0f);
	ImGui::DragFloat("Arc Start(deg)", &m_swing.ArcStartDeg, 1.0f, -360.0f, 360.0f);
	ImGui::DragFloat("Arc End(deg)", &m_swing.ArcEndDeg, 1.0f, -360.0f, 360.0f);
	ImGui::DragFloat("Radius", &m_swing.Radius, 0.05f, 0.1f, 10.0f);
	ImGui::DragFloat("Sword Length", &m_swing.SwordLength, 0.05f, 0.1f, 10.0f);
	ImGui::SliderFloat("Plane Tilt(deg)", &m_swing.PlaneTiltDeg, 0.0f, 90.0f);

	ImGui::Text("Trails : %d", static_cast<int>(m_objects.size()));
	if (m_dirty) { ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "* 未保存の変更あり"); }

	ImGui::End();
}

void SlashTrailEditor::DrawHierarchy()
{
	ImGui::Begin("SlashTrail Hierarchy");

	if (ImGui::Button("+ Add")) { AddObject(); }
	ImGui::SameLine();
	if (ImGui::Button("- Remove")) { RemoveSelected(); }
	ImGui::SameLine();
	if (ImGui::Button("Copy")) { CopySelected(); }

	ImGui::Separator();

	for (int i = 0; i < static_cast<int>(m_objects.size()); i++)
	{
		const std::string label = m_objects[i].name + "##" + std::to_string(i);
		if (ImGui::Selectable(label.c_str(), m_selected == i))
		{
			Stop();
			m_selected = i;
			m_defStale = true;
		}
	}

	ImGui::End();
}

bool SlashTrailEditor::TextureCombo(const char* label, std::string& path)
{
	bool changed = false;

	if (ImGui::BeginCombo(label, path.empty() ? "(None)" : path.c_str()))
	{
		if (ImGui::Selectable("(None)", path.empty())) { path.clear(); changed = true; }

		for (const auto& file : m_textureFileList)
		{
			if (ImGui::Selectable(file.c_str(), path == file)) { path = file; changed = true; }
		}
		ImGui::EndCombo();
	}
	return changed;
}

void SlashTrailEditor::DrawInspector()
{
	ImGui::Begin("SlashTrail Inspector");

	SlashTrailObject* obj = Selected();
	if (!obj)
	{
		ImGui::TextDisabled("トレイルが選択されていません");
		ImGui::End();
		return;
	}

	if (!m_textureListLoaded)
	{
		m_textureFileList = EditorUtil::ScanTextureFiles(kTextureAssetRoot);
		m_textureListLoaded = true;
	}
	if (ImGui::Button("Refresh Textures")) { m_textureFileList = EditorUtil::ScanTextureFiles(kTextureAssetRoot); }

	SlashTrailParams& p = obj->params;
	bool edited = false;

	char nameBuf[128];
	strcpy_s(nameBuf, obj->name.c_str());
	if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) { obj->name = nameBuf; edited = true; }

	edited |= TextureCombo("Texture", p.TexturePath);

	if (ImGui::CollapsingHeader("Record", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PushID("Record");
		edited |= ImGui::DragFloat("Min Sample Distance", &p.Record.MinSampleDistance, 0.001f, 0.001f, 1.0f);
		edited |= DragUInt("Max Samples", p.Record.MaxSamples, 1.0f, 8, 4096);
		edited |= ImGui::DragFloat("Max Recording Time", &p.Record.MaxRecordingTime, 0.1f, 0.0f, 60.0f);
		edited |= ImGui::DragFloat("Speed Smoothing Time", &p.Record.SpeedSmoothingTime, 0.005f, 0.0f, 1.0f);
		ImGui::PopID();
	}

	if (ImGui::CollapsingHeader("Stroke / Width", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PushID("Stroke");
		edited |= ImGui::DragFloat("Length", &p.Stroke.Length, 0.1f, 0.1f, 100.0f);
		edited |= ImGui::DragFloat("Head Taper Length", &p.Stroke.HeadTaperLength, 0.05f, 0.0f, 20.0f);
		edited |= ImGui::DragFloat("Speed Reference", &p.Width.SpeedReference, 0.1f, 0.1f, 200.0f);
		edited |= ImGui::DragFloat("Min Scale", &p.Width.MinScale, 0.01f, 0.0f, 10.0f);
		edited |= ImGui::DragFloat("Max Scale", &p.Width.MaxScale, 0.01f, 0.0f, 10.0f);
		ImGui::PopID();
	}

	if (ImGui::CollapsingHeader("Spline"))
	{
		ImGui::PushID("Spline");
		edited |= ImGui::Checkbox("Enabled", &p.Spline.Enabled);
		edited |= DragUInt("Subdivisions", p.Spline.Subdivisions, 0.2f, 1, 64);
		edited |= DragUInt("Max Points", p.Spline.MaxPoints, 1.0f, 2, 4096);
		ImGui::PopID();
	}

	if (ImGui::CollapsingHeader("Dissolve", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PushID("Dissolve");
		edited |= ImGui::DragFloat("Time", &p.Dissolve.Time, 0.01f, 0.01f, 10.0f);
		edited |= TextureCombo("Noise Texture", p.Dissolve.NoiseTexturePath);
		edited |= ImGui::DragFloat("Noise Scale U(長さ)", &p.Dissolve.NoiseScaleU, 0.01f, 0.0f, 50.0f);
		edited |= ImGui::DragFloat("Noise Scale V(幅)", &p.Dissolve.NoiseScaleV, 0.01f, 0.0f, 50.0f);
		edited |= ImGui::DragFloat("Scroll U", &p.Dissolve.ScrollU, 0.01f, -10.0f, 10.0f);
		edited |= ImGui::DragFloat("Scroll V", &p.Dissolve.ScrollV, 0.01f, -10.0f, 10.0f);
		edited |= ImGui::SliderFloat("Softness", &p.Dissolve.Softness, 0.0f, 1.0f);
		edited |= ImGui::SliderFloat("Edge Width", &p.Dissolve.EdgeWidth, 0.0f, 0.5f);
		edited |= ImGui::ColorEdit3("Edge Color", &p.Dissolve.EdgeColor.x);
		edited |= ImGui::DragFloat("Edge Intensity", &p.Dissolve.EdgeIntensity, 0.05f, 0.0f, 20.0f);
		ImGui::PopID();
	}

	if (ImGui::CollapsingHeader("Ink(墨)", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PushID("Ink");
		edited |= ImGui::ColorEdit3("Cold Color", &p.Ink.ColdColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
		edited |= ImGui::ColorEdit3("Hot Color", &p.Ink.HotColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
		edited |= BlendModeCombo("Blend Mode", p.Ink.BlendMode);
		edited |= DrawPassEdit(p.Ink.DrawPassFlags);
		ImGui::PopID();
	}

	if (ImGui::CollapsingHeader("Core(芯)", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PushID("Core");
		edited |= ImGui::Checkbox("Enabled", &p.Core.Enabled);
		edited |= ImGui::SliderFloat("Width Scale", &p.Core.WidthScale, 0.0f, 1.0f);
		edited |= ImGui::ColorEdit3("Color", &p.Core.Color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
		edited |= ImGui::SliderFloat("Tail Alpha", &p.Core.TailAlpha, 0.0f, 1.0f);
		edited |= BlendModeCombo("Blend Mode", p.Core.BlendMode);
		edited |= DrawPassEdit(p.Core.DrawPassFlags);
		ImGui::PopID();
	}

	if (edited) { MarkEdited(); }

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// オブジェクト操作・Save/Load
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailEditor::AddObject()
{
	SlashTrailObject obj;
	obj.name = "SlashTrail" + std::to_string(m_objects.size());
	m_objects.push_back(std::move(obj));

	Stop();
	m_selected = static_cast<int>(m_objects.size()) - 1;
	MarkEdited();
}

void SlashTrailEditor::RemoveSelected()
{
	if (!Selected()) { return; }

	Stop();
	m_objects.erase(m_objects.begin() + m_selected);
	m_selected = -1;
	MarkEdited();
}

void SlashTrailEditor::CopySelected()
{
	SlashTrailObject* src = Selected();
	if (!src) { return; }

	SlashTrailObject obj;
	obj.name = src->name + "_copy";
	obj.params = src->params;
	m_objects.push_back(std::move(obj));

	Stop();
	m_selected = static_cast<int>(m_objects.size()) - 1;
	MarkEdited();
}

void SlashTrailEditor::Save(const std::string& path)
{
	SlashTrailDataFile data;
	data.Trails.reserve(m_objects.size());

	for (const auto& obj : m_objects) { data.Trails.push_back({ obj.name, obj.params }); }

	if (!SlashTrailDataLoader::Save(path, data))
	{
		KdDebugGUI::Instance().AddLog("SlashTrailEditor: 保存に失敗 %s\n", path.c_str());
		return;
	}

	// 同じJSONを読んでいる実行中のDispatcherへ、次のUpdateでの再ロードを要求する
	SlashTrailDispatcher::NotifyDataSaved(path);

	m_watcher.Sync(path);
	m_dirty = false;

	KdDebugGUI::Instance().AddLog("SlashTrailEditor: 保存しました %s\n", path.c_str());
}

void SlashTrailEditor::Load(const std::string& path)
{
	SlashTrailDataFile data;
	if (!SlashTrailDataLoader::Load(path, data))
	{
		KdDebugGUI::Instance().AddLog("SlashTrailEditor: 読み込み失敗 %s\n", path.c_str());
		return;
	}

	Stop();
	m_objects.clear();

	for (auto& def : data.Trails)
	{
		SlashTrailObject obj;
		obj.name = def.Name;
		obj.params = def.Params;
		m_objects.push_back(std::move(obj));
	}

	m_selected = -1;
	m_dirty = false;
	m_defStale = true;
	m_watcher.Sync(path);

	KdDebugGUI::Instance().AddLog("SlashTrailEditor: 読み込みました %s\n", path.c_str());
}

void SlashTrailEditor::CheckHotReload()
{
	if (!m_watcher.Poll(m_filePathBuf, Application::Instance().GetDeltaTime())) { return; }

	// 編集中の内容を黙って上書きしない
	if (m_dirty)
	{
		KdDebugGUI::Instance().AddLog("SlashTrailEditor: 外部変更を検知しましたが未保存の編集があるため再読込しません\n");
		return;
	}

	const int keepSelected = m_selected;

	Load(m_filePathBuf);

	if (keepSelected >= 0 && keepSelected < static_cast<int>(m_objects.size())) { m_selected = keepSelected; }
}
