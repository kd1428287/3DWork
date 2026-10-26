#include "Application/main.h"

#include "ActionEditor.h"
#include "Application/Definitions/Character/Enemy/EnemyDefinitionJson.h"
#include "Application/Core/EventBus/Events/SceneEvents.h"

#include "imgui_internal.h"

#include <cmath>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 内部ヘルパー
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
namespace
{
	constexpr float kSimStep = 1.0f / 60.0f;	// プレビューのシミュレーション刻み(秒の編集スナップも兼ねる)

	const char* const kPhaseNames[3] = { "Windup", "Active", "Recovery" };
	const ImU32 kPhaseColors[3] = { IM_COL32(70, 110, 200, 255), IM_COL32(200, 70, 70, 255), IM_COL32(80, 170, 100, 255) };
	const char* const kTelegraphNames[3] = { "None", "Normal", "Danger" };

	// 左:対象 / 中央:Inspector / 右:Preview / 下:Timeline
	void SetupDockLayout(ImGuiID dockspaceId, const ImVec2& size)
	{
		ImGui::DockBuilderRemoveNode(dockspaceId);
		ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspaceId, size);

		ImGuiID center = dockspaceId;
		ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.2f, nullptr, &center);
		ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.3f, nullptr, &center);
		ImGuiID preview = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.6f, nullptr, &center);

		ImGui::DockBuilderDockWindow("Action Targets", left);
		ImGui::DockBuilderDockWindow("Action Inspector", center);
		ImGui::DockBuilderDockWindow("Action Preview", preview);
		ImGui::DockBuilderDockWindow("Action Timeline", bottom);

		ImGui::DockBuilderFinish(dockspaceId);
	}

	MotionClipData& PhaseClip(AttackPhaseData& p, int i)
	{
		return (i == 0) ? p.windup : (i == 1) ? p.active : p.recovery;
	}

	const MotionClipData& PhaseClip(const AttackPhaseData& p, int i)
	{
		return (i == 0) ? p.windup : (i == 1) ? p.active : p.recovery;
	}

	float PhaseTotalTime(const AttackPhaseData& p)
	{
		float total = 0.0f;
		for (int i = 0; i < 3; ++i) { total += std::max(PhaseClip(p, i).duration, 0.0f); }
		return total;
	}

	// 時刻tが属するフェーズと、そのフェーズ開始からの経過秒
	int PhaseIndexAt(const AttackPhaseData& p, float t, float& local)
	{
		float acc = 0.0f;
		for (int i = 0; i < 3; ++i)
		{
			const float d = std::max(PhaseClip(p, i).duration, 0.0f);
			if (t < acc + d || i == 2)
			{
				local = t - acc;
				return i;
			}
			acc += d;
		}
		local = 0.0f;
		return 0;
	}

	float SnapSec(float v) { return std::max(0.0f, std::round(v / kSimStep) * kSimStep); }

	// クリップ全体のフレーム数(クリップ自身のFPS基準。startFrame/endFrameと同じ単位)
	int ClipTotalFrames(const KdAnimationData& a)
	{
		return (a.m_fps > 0) ? (int)std::round(a.m_maxLength / 60.0f * (float)a.m_fps) : (int)a.m_maxLength;
	}

	// endFrameが0以下ならクリップ末尾
	int ClipEndFrame(const MotionClipData& c, int total)
	{
		return (c.endFrame <= 0) ? total : std::min(c.endFrame, total);
	}

	nlohmann::ordered_json* FindComponent(nlohmann::ordered_json& node, const char* type)
	{
		if (!node.contains("components") || !node["components"].is_array()) return nullptr;

		for (auto& c : node["components"])
		{
			if (c.value("type", std::string()) == type) return &c;
		}
		return nullptr;
	}

	// ordered_json → json(nlohmann::json専用のfrom_jsonを使うための変換)
	nlohmann::json ToJson(const nlohmann::ordered_json& j)
	{
		return nlohmann::json::parse(j.dump());
	}

	Math::Vector3 ReadScale(const nlohmann::ordered_json& node)
	{
		try
		{
			if (node.contains("scale")) { return ToJson(node["scale"]).get<Math::Vector3>(); }
		}
		catch (const nlohmann::json::exception&) {}
		return Math::Vector3(1, 1, 1);
	}

	RootMotionAxis ParseAxis(const std::string& s, RootMotionAxis fallback)
	{
		if (s == "X") return RootMotionAxis::X;
		if (s == "Y") return RootMotionAxis::Y;
		if (s == "Z") return RootMotionAxis::Z;
		return fallback;
	}

	RootMotionConfig ReadRootMotionConfig(const nlohmann::json& j)
	{
		RootMotionConfig c;
		c.boneName = j.value("boneName", c.boneName);
		c.unitScale = j.value("unitScale", c.unitScale);
		c.forwardAxis = ParseAxis(j.value("forwardAxis", std::string()), c.forwardAxis);
		c.forwardSign = j.value("forwardSign", c.forwardSign);
		c.rightAxis = ParseAxis(j.value("rightAxis", std::string()), c.rightAxis);
		c.rightSign = j.value("rightSign", c.rightSign);
		c.extractRotation = j.value("extractRotation", c.extractRotation);
		c.yawSign = j.value("yawSign", c.yawSign);
		return c;
	}

	// AttachToBoneの計算(実行時と同じ。ColliderEditor::TryComputeBoneAttachedWorld参照)。
	// ボーン位置+回転で回したオフセット、回転=attachRot*ボーン回転、スケール=自分*親。ボーンのscaleは使わない
	Math::Matrix ComputeSlotWorld(const Math::Vector3& attachPos, const Math::Quaternion& attachRot,
		const Math::Vector3& selfScale, const Math::Vector3& ownerScale,
		const Math::Matrix& boneModel, const Math::Matrix& modelWorld)
	{
		Math::Vector3 boneScale, bonePos;
		Math::Quaternion boneRot;
		if (!(boneModel * modelWorld).Decompose(boneScale, boneRot, bonePos)) { return modelWorld; }

		const Math::Vector3 rotatedOffset = Math::Vector3::Transform(attachPos * ownerScale, boneRot);

		return Math::Matrix::CreateScale(selfScale * ownerScale)
			* Math::Matrix::CreateFromQuaternion(attachRot * boneRot)
			* Math::Matrix::CreateTranslation(bonePos + rotatedOffset);
	}
}

ActionEditor::ActionEditor()
{
	EditorPreviewViewport::Settings settings;
	settings.UseMaskRT = false;
	settings.InitialDistance = 8.0f;
	settings.MinDistance = 0.5f;
	settings.MaxDistance = 60.0f;
	settings.WheelSensitivity = 0.6f;
	m_preview.Configure(settings);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 毎フレームの更新(ImGuiウィンドウ + 再生の進行)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ActionEditor::Update()
{
	const bool wasFocused = m_focused;
	m_focused = false;

	ImGuiIO& io = ImGui::GetIO();

	if (wasFocused && !io.WantTextInput && io.KeyCtrl)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) { Undo(); }
		if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) { Redo(); }
	}

	if (m_playing)
	{
		if (EnemyAttackDefinition* atk = CurrentAttack())
		{
			const float total = PhaseTotalTime(atk->attackData.phaseData);
			m_time += io.DeltaTime;
			if (m_time >= total)
			{
				if (m_loop) { m_time = 0.0f; }
				else { m_time = total; m_playing = false; }
			}
			m_poseDirty = true;
		}
		else
		{
			m_playing = false;
		}
	}

	ImGuiViewport* mainViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(
		ImVec2(mainViewport->Pos.x + mainViewport->Size.x + 20.0f, mainViewport->Pos.y + 40.0f),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(1200.0f, 760.0f), ImGuiCond_FirstUseEver);

	ImGui::Begin("Action Editor Window");
	{
		ImGuiID dockId = ImGui::GetID("ActionDockSpace");
		if (ImGui::DockBuilderGetNode(dockId) == nullptr)
		{
			SetupDockLayout(dockId, ImGui::GetContentRegionAvail());
		}
		ImGui::DockSpace(dockId, ImVec2(0, 0));
	}
	ImGui::End();

	DrawTargets();
	DrawInspector();
	DrawTimeline();
	DrawPreviewWindow();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 対象一覧ウィンドウ(Prefab読み込み・保存・攻撃一覧)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ActionEditor::DrawTargets()
{
	ImGui::Begin("Action Targets");
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) { m_focused = true; }

	ImGui::InputText("Prefab", m_prefabPathBuf, sizeof(m_prefabPathBuf));
	if (ImGui::Button("Load")) { LoadPrefab(m_prefabPathBuf); }

	ImGui::SameLine();
	ImGui::BeginDisabled(!m_loaded || !m_dirty);
	if (ImGui::Button("Save")) { Save(); }
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(m_undoStack.empty());
	if (ImGui::Button("Undo")) { Undo(); }
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(m_redoStack.empty());
	if (ImGui::Button("Redo")) { Redo(); }
	ImGui::EndDisabled();

	if (!m_loaded)
	{
		ImGui::TextDisabled("Prefabを読み込んでください");
		ImGui::End();
		return;
	}

	ImGui::TextDisabled("%s%s", m_tablePath.c_str(), m_dirty ? " *" : "");
	ImGui::Separator();

	DrawPoolList("Attacks", Pool::Attacks);
	DrawPoolList("Gap Closers", Pool::GapClosers);

	ImGui::End();
}

void ActionEditor::DrawPoolList(const char* title, Pool pool)
{
	std::vector<EnemyAttackDefinition>& vec = PoolVec(pool);

	ImGui::PushID(title);
	ImGui::TextDisabled("%s", title);

	for (int i = 0; i < (int)vec.size(); ++i)
	{
		ImGui::PushID(i);
		if (ImGui::Selectable(vec[i].name.c_str(), m_pool == pool && m_index == i)) { SelectAttack(pool, i); }
		ImGui::PopID();
	}

	const bool hasSelection = (m_pool == pool && CurrentAttack() != nullptr);

	// 追加は選択中の技の複製(無ければ既定値)
	if (ImGui::Button("Add"))
	{
		EnemyAttackDefinition def = hasSelection ? *CurrentAttack() : EnemyAttackDefinition();
		def.name = MakeUniqueName(def.name + "_Copy");

		PushUndo();
		vec.push_back(def);
		SelectAttack(pool, (int)vec.size() - 1);
		MarkEdited();
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!hasSelection);
	if (ImGui::Button("Delete"))
	{
		PushUndo();
		vec.erase(vec.begin() + m_index);
		ClampSelection();
		m_time = 0.0f;
		m_playing = false;
		MarkEdited();
	}
	ImGui::EndDisabled();

	ImGui::PopID();
	ImGui::Spacing();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Inspector(選択中の攻撃の全項目)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ActionEditor::DrawInspector()
{
	ImGui::Begin("Action Inspector");
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) { m_focused = true; }

	EnemyAttackDefinition* atk = CurrentAttack();
	if (atk == nullptr)
	{
		ImGui::TextDisabled("攻撃を選択してください");
		ImGui::End();
		return;
	}

	AttackData& d = atk->attackData;

	// 変更の開始(IsItemActivated)でUndo用スナップショットを取る
	auto dragFloat = [&](const char* label, float& v, float speed, float mn, float mx)
		{
			if (ImGui::DragFloat(label, &v, speed, mn, mx, "%.3f")) { MarkEdited(); }
			if (ImGui::IsItemActivated()) { PushUndo(); }
		};
	auto dragFloat3 = [&](const char* label, Math::Vector3& v, float speed)
		{
			if (ImGui::DragFloat3(label, &v.x, speed)) { MarkEdited(); }
			if (ImGui::IsItemActivated()) { PushUndo(); }
		};
	auto inputInt = [&](const char* label, int& v)
		{
			if (ImGui::InputInt(label, &v)) { v = std::max(v, 0); MarkEdited(); }
			if (ImGui::IsItemActivated()) { PushUndo(); }
		};
	auto checkbox = [&](const char* label, bool& v)
		{
			bool b = v;
			if (ImGui::Checkbox(label, &b)) { PushUndo(); v = b; MarkEdited(); }
		};
	auto inputText = [&](const char* label, std::string& s)
		{
			char buf[128];
			snprintf(buf, sizeof(buf), "%s", s.c_str());
			if (ImGui::InputText(label, buf, sizeof(buf))) { s = buf; MarkEdited(); }
			if (ImGui::IsItemActivated()) { PushUndo(); }
		};

	inputText("name", atk->name);

	if (ImGui::CollapsingHeader("Selection", ImGuiTreeNodeFlags_DefaultOpen))
	{
		dragFloat("minRange", atk->minRange, 0.05f, 0.0f, 100.0f);
		dragFloat("maxRange", atk->maxRange, 0.05f, 0.0f, 100.0f);
		dragFloat("weight", atk->weight, 0.05f, 0.0f, 100.0f);
		dragFloat("windupTurnSpeed", atk->facing.windupTurnSpeed, 0.1f, 0.0f, 50.0f);
		dragFloat("activeTurnSpeed", atk->facing.activeTurnSpeed, 0.1f, 0.0f, 50.0f);
	}

	if (ImGui::CollapsingHeader("Phases", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (int i = 0; i < 3; ++i)
		{
			MotionClipData& clip = PhaseClip(d.phaseData, i);

			ImGui::PushID(i);
			const bool open = ImGui::CollapsingHeader(kPhaseNames[i], ImGuiTreeNodeFlags_DefaultOpen);
			if (ImGui::IsItemClicked()) { m_selectedPhase = i; }

			if (open)
			{
				inputText("animation", clip.animationName);

				if (ImGui::BeginCombo("candidates", "選択..."))
				{
					for (const std::string& n : m_animNames)
					{
						if (ImGui::Selectable(n.c_str(), n == clip.animationName))
						{
							PushUndo();
							clip.animationName = n;
							MarkEdited();
						}
					}
					ImGui::EndCombo();
				}

				inputInt("startFrame", clip.startFrame);
				inputInt("endFrame (0=末尾)", clip.endFrame);
				dragFloat("duration (秒)", clip.duration, 0.01f, 0.0f, 20.0f);
				checkbox("useRootMotion", clip.useRootMotion);
				dragFloat("blendDuration", clip.blendDuration, 0.01f, 0.0f, 2.0f);

				// 実効再生速度(クリップの尺 ÷ duration)。極端な値はここで気付ける
				const auto anim = m_model.IsEnable() ? m_model.GetAnimation(clip.animationName) : nullptr;
				if (anim)
				{
					const int total = ClipTotalFrames(*anim);
					const int endF = ClipEndFrame(clip, total);
					const float clipSec = (float)(endF - clip.startFrame) / (float)std::max(anim->m_fps, 1);
					const float speed = (clip.duration > 0.0f) ? clipSec / clip.duration : 0.0f;
					const bool warn = clip.duration <= 0.0f || speed < 0.5f || speed > 2.0f;

					ImGui::TextColored(warn ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
						"%d / %d frames  %.3fs -> x%.2f", endF - clip.startFrame, total, clipSec, speed);
				}
				else
				{
					ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "アニメーションが見つかりません");
				}
			}
			ImGui::PopID();
		}
	}

	if (ImGui::CollapsingHeader("Telegraph"))
	{
		int type = (int)atk->telegraph.type;
		if (ImGui::Combo("type", &type, kTelegraphNames, 3))
		{
			PushUndo();
			atk->telegraph.type = (TelegraphType)type;
			MarkEdited();
		}
		inputText("effectName", atk->telegraph.effectName);

		if (ImGui::BeginCombo("anchorSlot", atk->telegraph.anchorSlot.empty() ? "(root)" : atk->telegraph.anchorSlot.c_str()))
		{
			if (ImGui::Selectable("(root)", atk->telegraph.anchorSlot.empty()))
			{
				PushUndo();
				atk->telegraph.anchorSlot.clear();
				MarkEdited();
			}
			for (const SlotInfo& s : m_slots)
			{
				if (ImGui::Selectable(s.slot.c_str(), s.slot == atk->telegraph.anchorSlot))
				{
					PushUndo();
					atk->telegraph.anchorSlot = s.slot;
					MarkEdited();
				}
			}
			ImGui::EndCombo();
		}
		dragFloat3("offset", atk->telegraph.offset, 0.05f);
		dragFloat("leadTime", atk->telegraph.leadTime, 0.01f, 0.0f, 5.0f);
	}

	if (ImGui::CollapsingHeader("Move"))
	{
		dragFloat("stepDistance", d.moveData.stepDistance, 0.05f, 0.0f, 50.0f);
		dragFloat("stepDuration", d.moveData.stepDuration, 0.01f, 0.0f, 5.0f);
		dragFloat("engageDistance", d.moveData.engageDistance, 0.05f, 0.0f, 50.0f);
		dragFloat3("stepDirection", d.moveData.stepDirection, 0.05f);
	}

	if (ImGui::CollapsingHeader("Weapon Slots", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (const SlotInfo& s : m_slots)
		{
			auto it = std::find(d.weaponSlots.begin(), d.weaponSlots.end(), s.slot);
			bool on = (it != d.weaponSlots.end());

			if (ImGui::Checkbox(s.slot.c_str(), &on))
			{
				PushUndo();
				if (on) { d.weaponSlots.push_back(s.slot); }
				else { d.weaponSlots.erase(it); }
				MarkEdited();
			}
		}
	}

	if (ImGui::CollapsingHeader("Damage"))
	{
		AttackDamageData& dd = d.damageData;
		dragFloat("damage", dd.damage, 0.1f, 0.0f, 1000.0f);
		dragFloat("postureDamage", dd.postureDamage, 0.1f, 0.0f, 1000.0f);
		dragFloat("guardDamageRatio", dd.guardDamageRatio, 0.01f, 0.0f, 1.0f);
		dragFloat("guardPostureDamageRatio", dd.guardPostureDamageRatio, 0.01f, 0.0f, 1.0f);
		dragFloat("parryPostureDamage", dd.parryPostureDamage, 0.1f, 0.0f, 1000.0f);
		checkbox("isParryable", dd.isParryable);
		checkbox("isReboundable", dd.isReboundable);
	}

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Timeline(ゲーム時間の帯 + クリップのフレーム帯)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ActionEditor::DrawTimeline()
{
	ImGui::Begin("Action Timeline");
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) { m_focused = true; }

	EnemyAttackDefinition* atk = CurrentAttack();
	if (atk == nullptr)
	{
		ImGui::TextDisabled("攻撃を選択してください");
		ImGui::End();
		return;
	}

	const float total = PhaseTotalTime(atk->attackData.phaseData);

	if (ImGui::Button(m_playing ? "Pause" : "Play"))
	{
		if (!m_playing && m_time >= total) { m_time = 0.0f; }
		m_playing = !m_playing;
		m_poseDirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop"))
	{
		m_playing = false;
		m_time = 0.0f;
		m_poseDirty = true;
	}
	ImGui::SameLine();
	ImGui::Checkbox("Loop", &m_loop);
	ImGui::SameLine();
	if (ImGui::Checkbox("Show inactive hitbox", &m_showInactiveHitbox)) {}
	ImGui::SameLine();
	ImGui::Text("%.3f / %.3f s  (%d / %d f @60)", m_time, total, (int)std::round(m_time * 60.0f), (int)std::round(total * 60.0f));

	DrawGameTimeStrip(*atk);
	ImGui::Spacing();
	DrawClipStrip(*atk);

	ImGui::End();
}

void ActionEditor::DrawGameTimeStrip(EnemyAttackDefinition& atk)
{
	AttackPhaseData& ph = atk.attackData.phaseData;
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImGuiIO& io = ImGui::GetIO();

	const float total = PhaseTotalTime(ph);
	const float width = std::max(ImGui::GetContentRegionAvail().x - 8.0f, 100.0f);
	constexpr float kHeight = 44.0f;
	const ImVec2 origin = ImGui::GetCursorScreenPos();

	// ドラッグ中に全体の長さが変わっても境界がマウスからずれないよう、表示スケールを固定する
	if (!m_draggingTimeline) { m_viewSec = std::max(total * 1.1f, 1.0f); }
	const float pxPerSec = width / m_viewSec;

	// 背景クリックでシーク、フェーズ選択
	ImGui::InvisibleButton("##scrub", ImVec2(width, kHeight));
	const float mouseSec = (io.MousePos.x - origin.x) / pxPerSec;
	if (ImGui::IsItemClicked())
	{
		float local = 0.0f;
		m_selectedPhase = PhaseIndexAt(ph, std::max(mouseSec, 0.0f), local);
	}
	if (ImGui::IsItemActive())
	{
		m_time = std::clamp(mouseSec, 0.0f, total);
		m_playing = false;
		m_poseDirty = true;
	}

	// 目盛り
	for (float s = 0.0f; s <= m_viewSec; s += 0.5f)
	{
		const float x = origin.x + s * pxPerSec;
		const bool whole = (std::fmod(s, 1.0f) < 0.01f);
		dl->AddLine(ImVec2(x, origin.y + kHeight), ImVec2(x, origin.y + kHeight + (whole ? 6.0f : 3.0f)), IM_COL32(150, 150, 150, 255));
	}

	// フェーズ帯
	float acc = 0.0f;
	for (int i = 0; i < 3; ++i)
	{
		const MotionClipData& clip = PhaseClip(ph, i);
		const float dur = std::max(clip.duration, 0.0f);
		const ImVec2 p0(origin.x + acc * pxPerSec, origin.y);
		const ImVec2 p1(origin.x + (acc + dur) * pxPerSec, origin.y + kHeight);

		dl->AddRectFilled(p0, p1, kPhaseColors[i]);
		dl->AddRect(p0, p1, (i == m_selectedPhase) ? IM_COL32(255, 255, 255, 255) : IM_COL32(20, 20, 20, 255), 0.0f, 0, (i == m_selectedPhase) ? 2.0f : 1.0f);

		char text[96];
		snprintf(text, sizeof(text), "%s  %.2fs\n[%d-%d]f", kPhaseNames[i], dur, clip.startFrame, clip.endFrame);
		dl->PushClipRect(p0, p1, true);
		dl->AddText(ImVec2(p0.x + 4.0f, p0.y + 3.0f), IM_COL32(255, 255, 255, 255), text);
		dl->PopClipRect();

		acc += dur;
	}

	// 予備動作エフェクトの発生位置(Windup終了のleadTime前)
	if (atk.telegraph.type != TelegraphType::None)
	{
		const float tt = std::max(ph.windup.duration - atk.telegraph.leadTime, 0.0f);
		const float x = origin.x + tt * pxPerSec;
		dl->AddLine(ImVec2(x, origin.y - 6.0f), ImVec2(x, origin.y + kHeight), IM_COL32(255, 220, 60, 255), 2.0f);
		dl->AddText(ImVec2(x + 3.0f, origin.y - 18.0f), IM_COL32(255, 220, 60, 255), "Telegraph");
	}

	// 再生位置
	const float px = origin.x + m_time * pxPerSec;
	dl->AddLine(ImVec2(px, origin.y - 4.0f), ImVec2(px, origin.y + kHeight + 4.0f), IM_COL32(255, 255, 255, 255), 2.0f);

	// 各フェーズ終端のドラッグでdurationを変える(秒は1/60刻み)
	acc = 0.0f;
	for (int i = 0; i < 3; ++i)
	{
		MotionClipData& clip = PhaseClip(ph, i);
		const float start = acc;
		acc += std::max(clip.duration, 0.0f);

		ImGui::SetCursorScreenPos(ImVec2(origin.x + acc * pxPerSec - 4.0f, origin.y));
		ImGui::PushID(i);
		ImGui::InvisibleButton("##edge", ImVec2(8.0f, kHeight));

		if (ImGui::IsItemActivated()) { PushUndo(); m_draggingTimeline = true; }
		if (ImGui::IsItemActive())
		{
			clip.duration = std::max(SnapSec(mouseSec - start), kSimStep);
			MarkEdited();
		}
		if (ImGui::IsItemDeactivated()) { m_draggingTimeline = false; }
		if (ImGui::IsItemHovered() || ImGui::IsItemActive()) { ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW); }
		ImGui::PopID();
	}

	ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + kHeight + 10.0f));
	ImGui::Dummy(ImVec2(width, 0.0f));
}

void ActionEditor::DrawClipStrip(EnemyAttackDefinition& atk)
{
	AttackPhaseData& ph = atk.attackData.phaseData;
	MotionClipData& target = PhaseClip(ph, m_selectedPhase);

	const auto anim = m_model.IsEnable() ? m_model.GetAnimation(target.animationName) : nullptr;
	if (!anim)
	{
		ImGui::TextDisabled("クリップ表示: アニメーション '%s' が見つかりません", target.animationName.c_str());
		return;
	}

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImGuiIO& io = ImGui::GetIO();

	const int totalFrames = std::max(ClipTotalFrames(*anim), 1);
	const float width = std::max(ImGui::GetContentRegionAvail().x - 8.0f, 100.0f);
	constexpr float kHeight = 28.0f;
	const float pxPerFrame = width / (float)totalFrames;

	ImGui::Text("Clip: %s  (%d frames @%dfps)", target.animationName.c_str(), totalFrames, anim->m_fps);

	const ImVec2 origin = ImGui::GetCursorScreenPos();
	dl->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + kHeight), IM_COL32(40, 40, 40, 255));

	// 同じクリップを使うフェーズの範囲を塗る
	for (int i = 0; i < 3; ++i)
	{
		const MotionClipData& c = PhaseClip(ph, i);
		if (c.animationName != target.animationName) { continue; }

		const float x0 = origin.x + c.startFrame * pxPerFrame;
		const float x1 = origin.x + ClipEndFrame(c, totalFrames) * pxPerFrame;
		dl->AddRectFilled(ImVec2(x0, origin.y), ImVec2(x1, origin.y + kHeight), (kPhaseColors[i] & 0x00FFFFFF) | 0x90000000);
	}

	// ハンドル: 同じフレームの境界(Windup終端とActive始端など)は1つにまとめて連動して動かす
	struct Handle { std::vector<int*> ptrs; int frame; };
	std::vector<Handle> handles;
	auto addHandle = [&](int* p, int frame)
		{
			for (Handle& h : handles)
			{
				if (h.frame == frame) { h.ptrs.push_back(p); return; }
			}
			handles.push_back({ { p }, frame });
		};
	for (int i = 0; i < 3; ++i)
	{
		MotionClipData& c = PhaseClip(ph, i);
		if (c.animationName != target.animationName) { continue; }

		addHandle(&c.startFrame, c.startFrame);
		addHandle(&c.endFrame, ClipEndFrame(c, totalFrames));
	}

	for (Handle& h : handles)
	{
		const float x = origin.x + h.frame * pxPerFrame;

		ImGui::SetCursorScreenPos(ImVec2(x - 5.0f, origin.y));
		ImGui::PushID((void*)h.ptrs[0]);
		ImGui::InvisibleButton("##handle", ImVec2(10.0f, kHeight));

		if (ImGui::IsItemActivated()) { PushUndo(); }
		if (ImGui::IsItemActive())
		{
			const int f = std::clamp((int)std::round((io.MousePos.x - origin.x) / pxPerFrame), 0, totalFrames);
			for (int* p : h.ptrs) { *p = f; }
			h.frame = f;
			MarkEdited();
		}
		if (ImGui::IsItemHovered() || ImGui::IsItemActive()) { ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW); }
		ImGui::PopID();

		const float hx = origin.x + h.frame * pxPerFrame;
		dl->AddLine(ImVec2(hx, origin.y), ImVec2(hx, origin.y + kHeight), IM_COL32(255, 255, 255, 255), 2.0f);

		char text[16];
		snprintf(text, sizeof(text), "%d", h.frame);
		dl->AddText(ImVec2(hx + 3.0f, origin.y + 2.0f), IM_COL32(255, 255, 255, 255), text);
	}

	// 現在時刻のクリップ位置
	float local = 0.0f;
	const int idx = PhaseIndexAt(ph, m_time, local);
	const MotionClipData& cur = PhaseClip(ph, idx);
	if (cur.animationName == target.animationName && cur.duration > 0.0f)
	{
		const float endF = (float)ClipEndFrame(cur, totalFrames);
		const float frame = (float)cur.startFrame + std::clamp(local / cur.duration, 0.0f, 1.0f) * (endF - (float)cur.startFrame);
		const float x = origin.x + frame * pxPerFrame;
		dl->AddLine(ImVec2(x, origin.y - 3.0f), ImVec2(x, origin.y + kHeight + 3.0f), IM_COL32(255, 255, 0, 255), 2.0f);
	}

	ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + kHeight + 6.0f));
	ImGui::Dummy(ImVec2(width, 0.0f));
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// プレビュー
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ActionEditor::DrawPreviewWindow()
{
	m_preview.DrawWindow("Action Preview", 0, [this]()
		{
			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) { m_focused = true; }

			const EnemyAttackDefinition* atk = CurrentAttack();
			if (atk == nullptr) { return; }

			const AttackPhaseData& ph = atk->attackData.phaseData;
			float local = 0.0f;
			const int idx = PhaseIndexAt(ph, m_time, local);
			const MotionClipData& clip = PhaseClip(ph, idx);

			float frame = (float)clip.startFrame;
			if (const auto anim = m_model.IsEnable() ? m_model.GetAnimation(clip.animationName) : nullptr)
			{
				const int total = ClipTotalFrames(*anim);
				const float endF = (float)ClipEndFrame(clip, total);
				frame += (clip.duration > 0.0f) ? std::clamp(local / clip.duration, 0.0f, 1.0f) * (endF - frame) : 0.0f;
			}

			const float rootDist = std::sqrt(m_rootMotionAccum.x * m_rootMotionAccum.x + m_rootMotionAccum.z * m_rootMotionAccum.z);

			char text[160];
			snprintf(text, sizeof(text), "%s  t=%.3fs  clip frame %.1f  root motion %.2fm", kPhaseNames[idx], m_time, frame, rootDist);

			const ImVec2 pos = m_preview.GetScreenPos();
			ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x + 8.0f, pos.y + 8.0f), IM_COL32(255, 255, 255, 255), text);
		});
}

// 専用カメラ・専用バッファへ、モデルと判定ワイヤーを描画する
void ActionEditor::RenderPreviewViewport()
{
	if (!m_preview.IsReady()) { return; }

	if (m_poseDirty) { EvaluatePose(); }

	m_preview.Render(m_previewTarget, [this]()
		{
			if (m_model.IsEnable())
			{
				if (m_model.NeedCalcNodeMatrices()) { m_model.CalcNodeMatrices(); }

				auto& shader = KdShaderManager::Instance().m_StandardShader;
				shader.BeginGenerateDepthMapFromLight();
				shader.DrawModel(m_model, m_modelWorld);
				shader.EndGenerateDepthMapFromLight();
				shader.BeginLit();
				shader.DrawModel(m_model, m_modelWorld);
				shader.EndLit();
			}

			DrawHitboxWires();
		});
}

// 実行時と同じ手順(フェーズごとにクリップをduration秒へ収めて再生)で、時刻m_timeのポーズを作る。
// 毎回先頭から1/60秒刻みで進めるので、シークも逆再生も同じ結果になる(ブレンドは省略)
void ActionEditor::EvaluatePose()
{
	m_poseDirty = false;
	m_rootMotionAccum = Math::Vector3::Zero;
	m_modelWorld = Math::Matrix::CreateScale(m_rootScale);

	const EnemyAttackDefinition* atk = CurrentAttack();
	if (atk == nullptr || !m_model.IsEnable()) { return; }

	const AttackPhaseData& ph = atk->attackData.phaseData;
	float local = 0.0f;
	m_phaseAtTime = PhaseIndexAt(ph, m_time, local);

	KdAnimator animator;
	RootMotionExtractor rootMotion;
	rootMotion.SetConfig(m_rootMotionConfig);

	float remaining = m_time;
	std::string prevAnim;

	for (int i = 0; i < 3; ++i)
	{
		const MotionClipData& clip = PhaseClip(ph, i);
		const float duration = std::max(clip.duration, kSimStep);

		if (const auto anim = m_model.GetAnimation(clip.animationName))
		{
			animator.SetAnimation(anim, false, clip.startFrame, clip.endFrame);
			rootMotion.SetActive(clip.useRootMotion);
			rootMotion.NotifyAnimationChanged(clip.animationName == prevAnim);
			prevAnim = clip.animationName;

			// Playと同じ速度スケーリング(クリップの尺 ÷ duration)
			const float speed = animator.GetDuration() / duration;
			const int steps = (int)(std::min(remaining, duration) / kSimStep + 0.0001f) + 1;

			for (int s = 0; s < steps; ++s)
			{
				rootMotion.PrepareFrame(m_model);
				animator.AdvanceTime(m_model.WorkNodes(), kSimStep * speed);
				rootMotion.FinalizeFrame(false);
				m_rootMotionAccum += rootMotion.ConsumeDelta();
			}
		}

		remaining -= duration;
		if (remaining < 0.0f) { break; }
	}

	m_modelWorld = Math::Matrix::CreateScale(m_rootScale) * Math::Matrix::CreateTranslation(m_rootMotionAccum);
}

// 選択中の攻撃のweaponSlotsのHitBoxを、現在のポーズに追従させて描く。Activeの間は赤、それ以外は灰
void ActionEditor::DrawHitboxWires()
{
	const EnemyAttackDefinition* atk = CurrentAttack();
	if (atk == nullptr || !m_model.IsEnable()) { return; }

	const bool isActive = (m_phaseAtTime == 1);
	if (!isActive && !m_showInactiveHitbox) { return; }

	const Math::Color col = isActive ? kRedColor : Math::Color(0.5f, 0.5f, 0.5f, 1.0f);

	for (const std::string& slotName : atk->attackData.weaponSlots)
	{
		const SlotInfo* slot = FindSlot(slotName);
		if (slot == nullptr) { continue; }

		const KdModelWork::Node* node = m_model.FindNode(slot->bone);
		if (node == nullptr) { continue; }

		const Math::Matrix world = ComputeSlotWorld(slot->attachPos, slot->attachRot, slot->selfScale, m_rootScale,
			node->m_worldTransform, m_modelWorld);

		for (const ColliderShapeParams& s : slot->hitShapes)
		{
			WireFrameComponent::AddShapeWire(m_wire, ToPreviewEntry(s), world, col);
		}
	}

	m_wire.Draw();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Prefab・攻撃テーブルの読み込みと保存
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool ActionEditor::LoadPrefab(const std::string& path)
{
	nlohmann::ordered_json root;
	if (!JsonLoader::Load(path, root))
	{
		KdDebugGUI::Instance().AddLog("ActionEditor: Prefab読み込み失敗 %s\n", path.c_str());
		return false;
	}

	// 攻撃テーブルのパスはEnemyAIControllerのparamsから取る
	std::string tablePath;
	if (nlohmann::ordered_json* ai = FindComponent(root, "EnemyAIController"))
	{
		if (ai->contains("params")) { tablePath = (*ai)["params"].value("attackTablePath", std::string()); }
	}
	if (tablePath.empty())
	{
		KdDebugGUI::Instance().AddLog("ActionEditor: EnemyAIControllerのattackTablePathが見つかりません\n");
		return false;
	}

	EnemyAttackTable table;
	if (!EnemyAttackTableLoader::LoadFromFile(tablePath, table))
	{
		KdDebugGUI::Instance().AddLog("ActionEditor: 攻撃テーブル読み込み失敗 %s\n", tablePath.c_str());
		return false;
	}

	// 保存時に残す_commentだけ、生のJSONから拾う
	m_tableComment.clear();
	nlohmann::ordered_json tableRoot;
	if (JsonLoader::Load(tablePath, tableRoot)) { m_tableComment = tableRoot.value("_comment", std::string()); }

	// モデル・アニメーションフォルダ
	std::string modelPath, animDir;
	if (nlohmann::ordered_json* mr = FindComponent(root, "ModelRender"))
	{
		if (mr->contains("params"))
		{
			modelPath = (*mr)["params"].value("model", std::string());
			animDir = (*mr)["params"].value("animations", std::string());
		}
	}

	// ルートモーション設定(ModelAnimatorのparams)
	m_rootMotionConfig = RootMotionConfig();
	if (nlohmann::ordered_json* ma = FindComponent(root, "ModelAnimator"))
	{
		if (ma->contains("params") && (*ma)["params"].contains("rootMotion"))
		{
			m_rootMotionConfig = ReadRootMotionConfig(ToJson((*ma)["params"]["rootMotion"]));
		}
	}

	// 武器スロット(Weaponコンポーネントを持つ子)
	m_rootScale = ReadScale(root);
	m_slots.clear();
	if (root.contains("children") && root["children"].is_array())
	{
		for (auto& child : root["children"])
		{
			nlohmann::ordered_json* weapon = FindComponent(child, "Weapon");
			if (weapon == nullptr) { continue; }

			SlotInfo s;
			s.slot = weapon->contains("params") ? (*weapon)["params"].value("slot", std::string("Main")) : std::string("Main");	// 未指定は"Main"とみなす
			s.selfScale = ReadScale(child);

			if (nlohmann::ordered_json* attach = FindComponent(child, "AttachToBone"))
			{
				const nlohmann::json params = attach->contains("params") ? ToJson((*attach)["params"]) : nlohmann::json::object();
				s.bone = params.value("bone", std::string());
				try
				{
					if (params.contains("position")) { s.attachPos = params["position"].get<Math::Vector3>(); }
					if (params.contains("rotationEulerDeg") && params["rotationEulerDeg"].is_array() && params["rotationEulerDeg"].size() >= 3)
					{
						const auto& e = params["rotationEulerDeg"];
						s.attachRot = Math::Quaternion::CreateFromYawPitchRoll(
							DirectX::XMConvertToRadians(e[1].get<float>()),
							DirectX::XMConvertToRadians(e[0].get<float>()),
							DirectX::XMConvertToRadians(e[2].get<float>()));
					}
				}
				catch (const nlohmann::json::exception&) {}
			}

			if (nlohmann::ordered_json* col = FindComponent(child, "Collider"))
			{
				try
				{
					if (col->contains("params"))
					{
						const ColliderParams cp = ToJson((*col)["params"]).get<ColliderParams>();
						for (const ColliderShapeParams& shape : cp.shapes)
						{
							if (shape.name == "HitBox") { s.hitShapes.push_back(shape); }
						}
					}
				}
				catch (const nlohmann::json::exception&) {}
			}

			m_slots.push_back(std::move(s));
		}
	}

	m_table = std::move(table);
	m_tablePath = tablePath;
	CollectAnimNames();

	// モデル(アニメーションが未登録の時だけフォルダを読み込む)
	if (modelPath != m_modelPath || !m_model.IsEnable())
	{
		m_modelPath = modelPath;
		m_model = KdModelWork();

		if (std::shared_ptr<KdModelData> data = KdAssets::Instance().m_modeldatas.GetData(modelPath))
		{
			m_model.SetModelData(data);

			const bool hasAnim = !m_animNames.empty() && m_model.GetAnimation(m_animNames[0]);
			if (!hasAnim && !animDir.empty()) { data->LoadAnimationDirectory(animDir); }
		}
		else
		{
			KdDebugGUI::Instance().AddLog("ActionEditor: モデル読み込み失敗 %s\n", modelPath.c_str());
		}
	}

	m_previewTarget = Math::Vector3(0.0f, 0.8f * m_rootScale.y, 0.0f);
	m_undoStack.clear();
	m_redoStack.clear();
	m_dirty = false;
	m_loaded = true;

	m_pool = Pool::Attacks;
	m_index = -1;
	SelectAttack(Pool::Attacks, m_table.attacks.empty() ? -1 : 0);

	KdDebugGUI::Instance().AddLog("ActionEditor: 読み込みました %s\n", tablePath.c_str());
	return true;
}

void ActionEditor::Save()
{
	if (!m_loaded) { return; }

	nlohmann::ordered_json out;
	if (!m_tableComment.empty()) { out["_comment"] = m_tableComment; }
	out["attacks"] = m_table.attacks;
	out["gapCloserAttacks"] = m_table.gapCloserAttacks;

	if (!JsonLoader::Save(m_tablePath, out))
	{
		KdDebugGUI::Instance().AddLog("ActionEditor: 保存失敗 %s\n", m_tablePath.c_str());
		return;
	}

	m_dirty = false;
	KdDebugGUI::Instance().AddLog("ActionEditor: 保存しました %s\n", m_tablePath.c_str());

	// 現在のシーンを再生成させる(ColliderEditorと同じ)
	GLOBALEVENT->Publish(Events::ReloadingScene());
}

// テーブル内で使われているクリップ名を、重複なしで集める(Inspectorの選択候補)
void ActionEditor::CollectAnimNames()
{
	m_animNames.clear();

	auto collect = [&](const std::vector<EnemyAttackDefinition>& vec)
		{
			for (const EnemyAttackDefinition& a : vec)
			{
				for (int i = 0; i < 3; ++i)
				{
					const std::string& n = PhaseClip(a.attackData.phaseData, i).animationName;
					if (!n.empty() && std::find(m_animNames.begin(), m_animNames.end(), n) == m_animNames.end())
					{
						m_animNames.push_back(n);
					}
				}
			}
		};
	collect(m_table.attacks);
	collect(m_table.gapCloserAttacks);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 選択・編集操作
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
std::vector<EnemyAttackDefinition>& ActionEditor::PoolVec(Pool pool)
{
	return (pool == Pool::Attacks) ? m_table.attacks : m_table.gapCloserAttacks;
}

EnemyAttackDefinition* ActionEditor::CurrentAttack()
{
	if (!m_loaded) { return nullptr; }

	std::vector<EnemyAttackDefinition>& vec = PoolVec(m_pool);
	return (m_index >= 0 && m_index < (int)vec.size()) ? &vec[m_index] : nullptr;
}

const ActionEditor::SlotInfo* ActionEditor::FindSlot(const std::string& slot) const
{
	for (const SlotInfo& s : m_slots)
	{
		if (s.slot == slot) { return &s; }
	}
	return nullptr;
}

std::string ActionEditor::MakeUniqueName(const std::string& base) const
{
	auto exists = [&](const std::string& n)
		{
			for (const EnemyAttackDefinition& a : m_table.attacks) { if (a.name == n) return true; }
			for (const EnemyAttackDefinition& a : m_table.gapCloserAttacks) { if (a.name == n) return true; }
			return false;
		};

	if (!exists(base)) { return base; }
	for (int i = 2;; ++i)
	{
		const std::string n = base + std::to_string(i);
		if (!exists(n)) { return n; }
	}
}

void ActionEditor::SelectAttack(Pool pool, int index)
{
	m_pool = pool;
	m_index = index;
	m_time = 0.0f;
	m_playing = false;
	m_poseDirty = true;
}

void ActionEditor::ClampSelection()
{
	const int count = (int)PoolVec(m_pool).size();
	if (m_index >= count) { m_index = count - 1; }
	m_poseDirty = true;
}

void ActionEditor::MarkEdited()
{
	m_dirty = true;
	m_poseDirty = true;
}

void ActionEditor::PushUndo()
{
	m_undoStack.push_back(m_table);
	if (m_undoStack.size() > kMaxUndoDepth) { m_undoStack.erase(m_undoStack.begin()); }
	m_redoStack.clear();
}

void ActionEditor::Undo()
{
	if (m_undoStack.empty()) { return; }

	m_redoStack.push_back(m_table);
	m_table = std::move(m_undoStack.back());
	m_undoStack.pop_back();

	ClampSelection();
	MarkEdited();
}

void ActionEditor::Redo()
{
	if (m_redoStack.empty()) { return; }

	m_undoStack.push_back(m_table);
	m_table = std::move(m_redoStack.back());
	m_redoStack.pop_back();

	ClampSelection();
	MarkEdited();
}
