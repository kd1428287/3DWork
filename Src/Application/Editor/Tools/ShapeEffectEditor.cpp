#include "Application/main.h"

#include "ShapeEffectEditor.h"
#include "../../Effect/Shape/ShapeEffectDispatcher.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace
{
	// EffectEditorと同じテクスチャルート(JSONにはルートからの相対パスで保存する)
	const std::string kTextureAssetRoot = "Asset/Textures/Game/Effect/";

	constexpr const char* kBlendNames[] = { "Alpha", "Add" };

	// Comboへ渡すEaseTypeの名前表(コールバック版はImGuiのバージョンで型が変わるため配列で渡す)
	const char* const* EaseNames()
	{
		static const char* names[(size_t)EaseType::Count] = {};
		static bool initialized = false;
		if (!initialized)
		{
			for (size_t i = 0; i < (size_t)EaseType::Count; ++i) { names[i] = EaseTypeName((EaseType)i); }
			initialized = true;
		}
		return names;
	}

	// Comboへ渡すビルボード手法の名前表
	const char* const* BillboardNames()
	{
		static const char* names[(size_t)ShapeBillboardMode::Count] = {};
		static bool initialized = false;
		if (!initialized)
		{
			for (size_t i = 0; i < (size_t)ShapeBillboardMode::Count; ++i) { names[i] = BillboardModeName((ShapeBillboardMode)i); }
			initialized = true;
		}
		return names;
	}
}

// ShapeEffectEditor専用ドックスペースの初期レイアウト
//	左：List / 中央：Inspector / 右上：Preview / 右下：Playback
static void SetupShapeEffectDockLayout(ImGuiID dockspaceId, const ImVec2& size)
{
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceId, size);

	ImGuiID center = dockspaceId;

	ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
	ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.50f, nullptr, &center);
	ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.35f, nullptr, &right);

	ImGui::DockBuilderDockWindow("ShapeEffect List", left);
	ImGui::DockBuilderDockWindow("ShapeEffect Inspector", center);
	ImGui::DockBuilderDockWindow("ShapeEffect Preview", right);
	ImGui::DockBuilderDockWindow("ShapeEffect Playback", rightBottom);

	ImGui::DockBuilderFinish(dockspaceId);
}

ShapeEffectEditor::ShapeEffectEditor()
{
	// ShapeEffectShaderはSV_Target0のみ書くのでMask RTは不要。Bright/エフェクト層はDrawPassの確認に使う
	EditorPreviewViewport::Settings settings;
	settings.UseBrightRT = true;
	settings.UseEffectLayerRT = true;
	settings.InitialDistance = 4.0f;
	m_preview.Configure(settings);

	Load(m_filePathBuf);
}

void ShapeEffectEditor::Update()
{
	AdvancePreview(Application::Instance().GetDeltaTime());

	// 専用のコンテナウィンドウ。メインビューポートの右外側へ初期配置することで、
	// マルチビューポート機能により起動時から別ウィンドウとして分離表示される(EffectEditorと同じ)
	ImGuiViewport* mainViewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos(
		ImVec2(mainViewport->Pos.x + mainViewport->Size.x + 60.0f, mainViewport->Pos.y + 40.0f),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(900.0f, 700.0f), ImGuiCond_FirstUseEver);

	ImGui::Begin("ShapeEffect Editor Window");
	{
		ImGuiID dockId = ImGui::GetID("ShapeEffectDockSpace");

		if (ImGui::DockBuilderGetNode(dockId) == nullptr)
		{
			SetupShapeEffectDockLayout(dockId, ImGui::GetContentRegionAvail());
		}

		ImGui::DockSpace(dockId, ImVec2(0, 0));
	}
	ImGui::End();

	DrawListWindow();
	DrawInspectorWindow();
	DrawPlaybackWindow();
	DrawPreviewWindow();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// プレビュー
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ShapeEffectEditor::RebuildPreview()
{
	m_motion.reset();
	if (!HasSelection()) { return; }

	const ShapeEffectData& effect = m_effects[m_selected];

	m_motion = std::make_shared<KeyframeMotion>();
	m_motion->keys = effect.Keys;

	m_previewInstance.Init(m_motion, effect.Params, &m_textureProvider);
	m_previewInstance.SetTime(m_time);
}

void ShapeEffectEditor::AdvancePreview(float deltaTime)
{
	if (!HasSelection() || !m_motion) { return; }

	const float duration = m_motion->GetDuration();

	if (m_playing && duration > 0.0f)
	{
		m_time += deltaTime;
		if (m_time >= duration)
		{
			if (m_loop) { m_time = std::fmod(m_time, duration); }
			else { m_time = duration; m_playing = false; }
		}
	}

	m_time = std::clamp(m_time, 0.0f, duration);
	m_previewInstance.SetTime(m_time);
}

Math::Matrix ShapeEffectEditor::GetPreviewBase() const
{
	const float yaw = DirectX::XMConvertToRadians(m_baseYawSpeedDeg * m_time);
	return Math::Matrix::CreateFromYawPitchRoll(yaw, 0.0f, 0.0f);
}

void ShapeEffectEditor::RenderPreviewViewport()
{
	if (!m_preview.IsReady()) { return; }

	const bool has = HasSelection() && m_motion;
	const Math::Matrix base = GetPreviewBase();

	// 本編のDispatcher::Draw()と同じく、DrawPassFlagsに含まれるパスでだけ描く
	auto drawPass = [&](ParticleDrawPass pass)
		{
			if (!has) { return; }
			if (!KdHasDrawPassFlag(m_effects[m_selected].Params.DrawPassFlags, pass)) { return; }

			m_previewInstance.Draw(base);
		};

	m_preview.Render({ 0.0f, 0.0f, 0.0f },
		nullptr,
		[&]() { drawPass(ParticleDrawPass::Bright); },
		[&]() { drawPass(ParticleDrawPass::Default); });
}

void ShapeEffectEditor::DrawPreviewWindow()
{
	m_preview.DrawWindow("ShapeEffect Preview", 0, [this]()
		{
			ImGui::SetCursorPos(ImVec2(10, 10));

			if (!HasSelection() || !m_motion)
			{
				ImGui::TextDisabled("エフェクトが選択されていません");
				return;
			}
			ImGui::Text("t = %.2f / %.2f s", m_time, m_motion->GetDuration());
		});
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エディタウィンドウ
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ShapeEffectEditor::DrawListWindow()
{
	ImGui::Begin("ShapeEffect List");

	ImGui::InputText("Path", m_filePathBuf, sizeof(m_filePathBuf));
	if (ImGui::Button("Save")) { Save(m_filePathBuf); }
	ImGui::SameLine();
	if (ImGui::Button("Load")) { Load(m_filePathBuf); }

	ImGui::Separator();

	if (ImGui::Button("+ Add")) { AddEffect(); }
	ImGui::SameLine();
	if (ImGui::Button("- Remove")) { RemoveSelected(); }
	ImGui::SameLine();
	if (ImGui::Button("Copy")) { CopySelected(); }

	ImGui::Separator();

	for (int i = 0; i < (int)m_effects.size(); ++i)
	{
		const std::string label = m_effects[i].Name + "##" + std::to_string(i);
		if (ImGui::Selectable(label.c_str(), m_selected == i)) { SelectEffect(i); }
	}

	ImGui::End();
}

void ShapeEffectEditor::DrawInspectorWindow()
{
	ImGui::Begin("ShapeEffect Inspector");

	if (!HasSelection())
	{
		ImGui::TextDisabled("エフェクトが選択されていません");
		ImGui::End();
		return;
	}

	ShapeEffectData& effect = m_effects[m_selected];

	bool changed = DrawParams(effect);

	ImGui::Separator();
	changed |= DrawKeys(effect);

	if (changed) { RebuildPreview(); }

	ImGui::End();
}

void ShapeEffectEditor::DrawPlaybackWindow()
{
	ImGui::Begin("ShapeEffect Playback");

	if (!HasSelection())
	{
		ImGui::TextDisabled("エフェクトが選択されていません");
		ImGui::End();
		return;
	}

	DrawPlayback();

	ImGui::End();
}

// 名前・テクスチャ・ブレンド・描画パス・色。プレビューの作り直しが必要な変更があればtrue
bool ShapeEffectEditor::DrawParams(ShapeEffectData& effect)
{
	bool changed = false;
	ShapeEffectParams& params = effect.Params;

	char nameBuf[128];
	strcpy_s(nameBuf, effect.Name.c_str());
	if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) { effect.Name = nameBuf; }
	if (IsNameDuplicated(effect.Name))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "同名のエフェクトがあります(読み込み時は後のものが優先)");
	}

	ImGui::Text("Texture : %s", params.TexturePath.empty() ? "(None)" : params.TexturePath.c_str());
	if (ImGui::TreeNode("Textures"))
	{
		changed |= DrawTexturePicker(params);
		ImGui::TreePop();
	}

	int blend = (params.BlendMode == ParticleBlendMode::Alpha) ? 0 : 1;
	if (ImGui::Combo("Blend Mode", &blend, kBlendNames, 2))
	{
		params.BlendMode = (blend == 0) ? ParticleBlendMode::Alpha : ParticleBlendMode::Add;
		changed = true;
	}

	bool useDefault = KdHasDrawPassFlag(params.DrawPassFlags, ParticleDrawPass::Default);
	bool useBright = KdHasDrawPassFlag(params.DrawPassFlags, ParticleDrawPass::Bright);
	ImGui::Text("Draw Pass");
	ImGui::SameLine();
	bool passChanged = ImGui::Checkbox("Default", &useDefault);
	ImGui::SameLine();
	passChanged |= ImGui::Checkbox("Bright", &useBright);
	if (passChanged)
	{
		ParticleDrawPass flags = static_cast<ParticleDrawPass>(0);
		if (useDefault) { flags |= ParticleDrawPass::Default; }
		if (useBright) { flags |= ParticleDrawPass::Bright; }
		params.DrawPassFlags = flags;
		changed = true;
	}

	changed |= ImGui::ColorEdit4("Color", &params.Color.x);

	int billboard = (int)params.Billboard;
	if (ImGui::Combo("Billboard", &billboard, BillboardNames(), (int)ShapeBillboardMode::Count))
	{
		params.Billboard = (ShapeBillboardMode)billboard;
		changed = true;
	}
	ImGui::TextDisabled("None=向き固定(基準点の回転に追従) / Full=常にカメラ正対(Rot Zのみ有効) / YAxis=Y軸回りだけカメラへ向く");

	changed |= ImGui::DragFloat("Depth Offset", &params.DepthOffset, 0.01f, -10.0f, 10.0f);
	changed |= ImGui::Checkbox("Depth Test", &params.DepthTest);
	ImGui::TextDisabled("Depth Offset：+で手前へ(位置・大きさは変えず深度だけずらす) / Depth Test：オフで手前の物に隠されない");

	ImGui::Separator();
	ImGui::Text("Noise");
	{
		ParticleNoiseParams& noise = params.Noise;

		bool useMask = (noise.flags & ParticleNoiseFlag_Mask) != 0;
		bool useDissolve = (noise.flags & ParticleNoiseFlag_Dissolve) != 0;

		bool flagsChanged = ImGui::Checkbox("Mask", &useMask);
		ImGui::SameLine();
		flagsChanged |= ImGui::Checkbox("Dissolve", &useDissolve);
		if (flagsChanged)
		{
			noise.flags = (useMask ? static_cast<UINT>(ParticleNoiseFlag_Mask) : 0u)
				| (useDissolve ? static_cast<UINT>(ParticleNoiseFlag_Dissolve) : 0u);
			changed = true;
		}

		if (noise.flags != 0)
		{
			ImGui::Text("Noise Texture : %s", params.NoiseTexturePath.empty() ? "(None)" : params.NoiseTexturePath.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Clear##NoiseTex")) { params.NoiseTexturePath.clear(); changed = true; }

			ImGui::TextDisabled("Texturesの「Target」をNoiseにして選択(タイリング可能な画像のRチャンネルを使用)");
			if (params.NoiseTexturePath.empty())
			{
				ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "ノイズテクスチャ未設定のため、ノイズは適用されません");
			}

			changed |= ImGui::DragFloat("Noise Scale", &noise.noiseScale, 0.05f, 0.01f, 32.0f);
			changed |= ImGui::DragFloat2("Noise Scroll", &noise.scroll.x, 0.01f, -10.0f, 10.0f);
			ImGui::TextDisabled("Scroll：1秒あたりのノイズUV移動量(パーティクルと違い、寿命ではなく時間で流れる)");

			if (useMask)
			{
				changed |= ImGui::SliderFloat("Mask Power", &noise.maskPower, 0.1f, 8.0f);
			}

			if (useDissolve)
			{
				changed |= ImGui::SliderFloat("Dissolve Start", &noise.dissolveStart, 0.0f, 0.99f);
				changed |= ImGui::SliderFloat("Dissolve Softness", &noise.dissolveSoftness, 0.001f, 0.3f);
				changed |= ImGui::SliderFloat("Edge Width", &noise.edgeWidth, 0.001f, 0.5f);
				changed |= ImGui::ColorEdit3("Edge Color", &noise.edgeColor.x);
				changed |= ImGui::DragFloat("Edge Intensity", &noise.edgeColor.w, 0.05f, 0.0f, 20.0f);
				ImGui::TextDisabled("Dissolve有効時は線形フェードの代わりに、Keyのalphaが進行度になる(1=見える、0=ノイズ順に消える。Startより上は消え始めない)");
			}
		}
	}

	return changed;
}

bool ShapeEffectEditor::DrawTexturePicker(ShapeEffectParams& params)
{
	if (!m_textureListLoaded)
	{
		RefreshTextureFileList();
		m_textureListLoaded = true;
	}

	if (ImGui::Button("Refresh")) { RefreshTextureFileList(); }
	ImGui::SameLine();
	ImGui::Text("Target");
	ImGui::SameLine();
	if (ImGui::RadioButton("Texture", !m_pickNoiseTexture)) { m_pickNoiseTexture = false; }
	ImGui::SameLine();
	if (ImGui::RadioButton("Noise", m_pickNoiseTexture)) { m_pickNoiseTexture = true; }

	std::string& target = m_pickNoiseTexture ? params.NoiseTexturePath : params.TexturePath;

	bool changed = false;

	ImGui::BeginChild("##ShapeTextureList", ImVec2(0, 120), true);
	for (const std::string& path : m_textureFileList)
	{
		if (ImGui::Selectable(path.c_str(), target == path))
		{
			target = path;
			changed = true;
		}
	}
	ImGui::EndChild();

	return changed;
}

void ShapeEffectEditor::DrawPlayback()
{
	const float duration = m_motion ? m_motion->GetDuration() : 0.0f;

	if (ImGui::Button(m_playing ? "Pause" : "Play"))
	{
		// 終端で止まっている場合は頭から再生し直す
		if (!m_playing && m_time >= duration) { m_time = 0.0f; }
		m_playing = !m_playing;
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop")) { m_playing = false; m_time = 0.0f; }
	ImGui::SameLine();
	ImGui::Checkbox("Loop", &m_loop);

	if (ImGui::SliderFloat("Time", &m_time, 0.0f, std::max(duration, 0.001f), "%.2f s")) { m_playing = false; }

	ImGui::DragFloat("Base Yaw (deg/s)", &m_baseYawSpeedDeg, 1.0f, -360.0f, 360.0f);
	ImGui::TextDisabled("0=基準点を固定 / 0以外=基準点がY軸回りに回転(基準点の回転への追従確認用)");
}

// キー列。追加・削除・複製・並べ替えは、ループ後にまとめて適用する
bool ShapeEffectEditor::DrawKeys(ShapeEffectData& effect)
{
	bool changed = false;
	std::vector<EffectKey>& keys = effect.Keys;

	ImGui::Text("Keys (%d)  Total %.2f s", (int)keys.size(), m_motion ? m_motion->GetDuration() : 0.0f);
	if (ImGui::Button("+ Add Key"))
	{
		EffectKey key = keys.empty() ? EffectKey{} : keys.back();
		if (!keys.empty()) { key.duration = 0.5f; }
		keys.push_back(key);
		changed = true;
	}

	int removeIndex = -1;
	int duplicateIndex = -1;
	int moveIndex = -1;
	int moveDir = 0;

	float reachTime = 0.0f;
	for (int i = 0; i < (int)keys.size(); ++i)
	{
		EffectKey& key = keys[i];
		if (i > 0) { reachTime += key.duration; }

		ImGui::PushID(i);

		char label[64];
		if (i == 0) { snprintf(label, sizeof(label), "Key 0 (初期姿勢)##key"); }
		else { snprintf(label, sizeof(label), "Key %d  (到達 %.2f s)##key", i, reachTime); }

		if (ImGui::TreeNode(label))
		{
			changed |= ImGui::DragFloat3("Pos", &key.pos.x, 0.01f);
			changed |= ImGui::DragFloat3("Rot (deg)", &key.rotDeg.x, 1.0f);
			changed |= ImGui::DragFloat2("Scale", &key.scale.x, 0.01f);
			changed |= ImGui::SliderFloat("Alpha", &key.alpha, 0.0f, 1.0f);

			if (i > 0)
			{
				int ease = (int)key.ease;
				if (ImGui::Combo("Ease", &ease, EaseNames(), (int)EaseType::Count))
				{
					key.ease = (EaseType)ease;
					changed = true;
				}
				changed |= ImGui::DragFloat("Duration", &key.duration, 0.01f, 0.0f, 60.0f);
			}

			if (i > 0 && ImGui::Button("Up")) { moveIndex = i; moveDir = -1; }
			if (i > 0) { ImGui::SameLine(); }
			if (i < (int)keys.size() - 1 && ImGui::Button("Down")) { moveIndex = i; moveDir = 1; }
			if (i < (int)keys.size() - 1) { ImGui::SameLine(); }
			if (ImGui::Button("Duplicate")) { duplicateIndex = i; }
			ImGui::SameLine();
			if (ImGui::Button("Remove")) { removeIndex = i; }

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	if (removeIndex >= 0)
	{
		keys.erase(keys.begin() + removeIndex);
		changed = true;
	}
	else if (duplicateIndex >= 0)
	{
		const EffectKey copy = keys[duplicateIndex];
		keys.insert(keys.begin() + duplicateIndex + 1, copy);
		changed = true;
	}
	else if (moveIndex >= 0)
	{
		std::swap(keys[moveIndex], keys[moveIndex + moveDir]);
		changed = true;
	}

	return changed;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 一覧の操作
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool ShapeEffectEditor::IsNameDuplicated(const std::string& name) const
{
	int count = 0;
	for (const ShapeEffectData& e : m_effects) { if (e.Name == name) { ++count; } }
	return count > 1;
}

std::string ShapeEffectEditor::MakeUniqueName(const std::string& base) const
{
	auto exists = [this](const std::string& name)
		{
			for (const ShapeEffectData& e : m_effects) { if (e.Name == name) { return true; } }
			return false;
		};

	if (!exists(base)) { return base; }

	for (int i = 1; ; ++i)
	{
		const std::string name = base + std::to_string(i);
		if (!exists(name)) { return name; }
	}
}

void ShapeEffectEditor::SelectEffect(int index)
{
	m_selected = index;
	m_time = 0.0f;
	m_playing = true;
	RebuildPreview();
}

void ShapeEffectEditor::AddEffect()
{
	ShapeEffectData effect;
	effect.Name = MakeUniqueName("ShapeEffect");

	EffectKey initial;
	initial.alpha = 0.0f;

	EffectKey reach;
	reach.ease = EaseType::OutQuad;
	reach.duration = 0.5f;

	effect.Keys = { initial, reach };

	m_effects.push_back(std::move(effect));
	SelectEffect((int)m_effects.size() - 1);
}

void ShapeEffectEditor::RemoveSelected()
{
	if (!HasSelection()) { return; }

	m_effects.erase(m_effects.begin() + m_selected);

	const int next = m_effects.empty() ? -1 : std::min(m_selected, (int)m_effects.size() - 1);
	m_selected = -1;
	if (next >= 0) { SelectEffect(next); }
	else { m_motion.reset(); m_playing = false; }
}

void ShapeEffectEditor::CopySelected()
{
	if (!HasSelection()) { return; }

	ShapeEffectData copy = m_effects[m_selected];
	copy.Name = MakeUniqueName(copy.Name + "_Copy");

	m_effects.push_back(std::move(copy));
	SelectEffect((int)m_effects.size() - 1);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 保存・読み込み
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ShapeEffectEditor::Save(const std::string& path)
{
	ShapeEffectDataFile data;
	data.Effects = m_effects;

	if (!ShapeEffectDataLoader::Save(path, data))
	{
		KdDebugGUI::Instance().AddLog("ShapeEffectEditor: 保存に失敗 %s\n", path.c_str());
		return;
	}

	// 実行中で、同じJSONを読み込んでいるShapeEffectDispatcherへ再ロードを通知する
	ShapeEffectDispatcher::NotifyDataSaved(path);

	KdDebugGUI::Instance().AddLog("ShapeEffectEditor: 保存しました %s\n", path.c_str());
}

void ShapeEffectEditor::Load(const std::string& path)
{
	ShapeEffectDataFile data;
	if (!ShapeEffectDataLoader::Load(path, data))
	{
		KdDebugGUI::Instance().AddLog("ShapeEffectEditor: 読み込み失敗 %s\n", path.c_str());
		return;
	}

	m_effects = std::move(data.Effects);
	m_selected = -1;
	m_motion.reset();
	m_playing = false;
	m_time = 0.0f;

	KdDebugGUI::Instance().AddLog("ShapeEffectEditor: 読み込みました %s\n", path.c_str());
}

void ShapeEffectEditor::RefreshTextureFileList()
{
	m_textureFileList.clear();

	namespace fs = std::filesystem;

	if (!fs::exists(kTextureAssetRoot)) { return; }

	static const std::vector<std::string> kExtensions = { ".png", ".dds", ".jpg", ".jpeg", ".tga" };

	for (auto& entry : fs::recursive_directory_iterator(kTextureAssetRoot))
	{
		if (!entry.is_regular_file()) { continue; }

		std::string ext = entry.path().extension().string();
		for (auto& c : ext) { c = (char)tolower(c); }

		if (std::find(kExtensions.begin(), kExtensions.end(), ext) == kExtensions.end()) { continue; }

		m_textureFileList.push_back(fs::relative(entry.path(), kTextureAssetRoot).generic_string());
	}
}
