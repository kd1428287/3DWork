#include "Application/main.h"

#include "ColliderEditor.h"
#include "MapEditor.h"
#include "Application/Definitions/Physics/ColliderCategoryNames.h"
#include "Application/Core/EventBus/Events/SceneEvents.h"

#include "imgui_internal.h"

#include <cfloat>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 内部ヘルパー
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
namespace
{
	const char* const kShapeNames[] = { "Sphere", "Capsule", "Box" };

	// ColliderEditor専用ドックスペースの初期レイアウト(左:対象 / 中央:Inspector / 右:Preview)
	void SetupDockLayout(ImGuiID dockspaceId, const ImVec2& size)
	{
		ImGui::DockBuilderRemoveNode(dockspaceId);
		ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspaceId, size);

		ImGuiID center = dockspaceId;
		ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.22f, nullptr, &center);
		ImGuiID preview = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.55f, nullptr, &center);

		ImGui::DockBuilderDockWindow("Collider Targets", left);
		ImGui::DockBuilderDockWindow("Collider Inspector", center);
		ImGui::DockBuilderDockWindow("Collider Preview", preview);

		ImGui::DockBuilderFinish(dockspaceId);
	}

	// ordered_jsonのGameObject定義から、指定typeのコンポーネントを探す
	nlohmann::ordered_json* FindComponent(nlohmann::ordered_json& node, const char* type)
	{
		if (!node.contains("components") || !node["components"].is_array()) return nullptr;

		for (auto& c : node["components"])
		{
			if (c.value("type", std::string()) == type) return &c;
		}
		return nullptr;
	}

	MapObject* FindMapObject(ObjectId id)
	{
		if (id == kInvalidObjectId) return nullptr;

		for (auto& o : MapEditor::Instance().WorkObjects())
		{
			if (o.data.id == id) return &o;
		}
		return nullptr;
	}

	ComponentEntry* FindMapComponent(MapObject& obj, const char* type)
	{
		for (auto& c : obj.data.components)
		{
			if (c.type == type) return &c;
		}
		return nullptr;
	}

	// ordered_json → json(nlohmann::json専用のfrom_jsonを使うための変換)
	nlohmann::json ToJson(const nlohmann::ordered_json& j)
	{
		return nlohmann::json::parse(j.dump());
	}

	// ノードのローカル行列(scale/rotation/pos)。無いキーは単位。scaleOnlyならスケールだけ
	//	rotationは長さ4ならQuaternion(x,y,z,w)、長さ3ならEuler角(度)として読む
	Math::Matrix ReadNodeLocalMatrix(const nlohmann::ordered_json& node, bool scaleOnly)
	{
		Math::Vector3 scale(1, 1, 1), pos(0, 0, 0);
		Math::Quaternion rot = Math::Quaternion::Identity;

		try
		{
			if (node.contains("scale")) { scale = ToJson(node["scale"]).get<Math::Vector3>(); }
			if (!scaleOnly && node.contains("pos")) { pos = ToJson(node["pos"]).get<Math::Vector3>(); }
			if (!scaleOnly && node.contains("rotation") && node["rotation"].is_array())
			{
				const auto& r = node["rotation"];
				if (r.size() >= 4)
				{
					rot = Math::Quaternion(r[0].get<float>(), r[1].get<float>(), r[2].get<float>(), r[3].get<float>());
				}
				else if (r.size() == 3)
				{
					rot = Math::Quaternion::CreateFromYawPitchRoll(
						DirectX::XMConvertToRadians(r[1].get<float>()),
						DirectX::XMConvertToRadians(r[0].get<float>()),
						DirectX::XMConvertToRadians(r[2].get<float>()));
				}
			}
		}
		catch (const nlohmann::json::exception&) {}

		return Math::Matrix::CreateScale(scale) * Math::Matrix::CreateFromQuaternion(rot) * Math::Matrix::CreateTranslation(pos);
	}

	// ノードのModelRenderコンポーネントのmodelパス(無ければ空)
	std::string ReadModelPath(nlohmann::ordered_json& node)
	{
		if (nlohmann::ordered_json* mr = FindComponent(node, "ModelRender"))
		{
			if (mr->contains("params")) { return (*mr)["params"].value("model", std::string()); }
		}
		return std::string();
	}

	Math::Vector3 ReadNodeScale(const nlohmann::ordered_json& node)
	{
		try
		{
			if (node.contains("scale")) { return ToJson(node["scale"]).get<Math::Vector3>(); }
		}
		catch (const nlohmann::json::exception&) {}
		return Math::Vector3(1, 1, 1);
	}

	// ボーン名 → モデル空間でのボーン行列(KdModelWorkのノード行列)。
	//	※実行時のSkeletonComponent::TryGetBoneWorldMatrix()の取り方に合わせること
	bool TryGetBoneModelMatrix(KdModelWork& work, const std::string& bone, Math::Matrix& out)
	{
		if (!work.IsEnable()) return false;

		if (work.NeedCalcNodeMatrices()) { work.CalcNodeMatrices(); }

		const KdModelWork::Node* node = work.FindNode(bone);
		if (!node) return false;

		out = node->m_worldTransform;
		return true;
	}
}

ColliderEditor::ColliderEditor()
{
	EditorPreviewViewport::Settings settings;
	settings.UseMaskRT = false;
	settings.InitialDistance = 4.0f;
	settings.MinDistance = 0.3f;
	settings.MaxDistance = 50.0f;
	settings.WheelSensitivity = 0.3f;
	m_preview.Configure(settings);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 毎フレームの更新(ImGuiウィンドウ)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ColliderEditor::Update()
{
	const bool wasFocused = m_focused;
	m_focused = false;

	// ギズモの操作切り替え(1:移動 / 3:スケール)。このエディタにフォーカスがある時だけ
	if (wasFocused && !ImGui::GetIO().WantTextInput && !ImGuizmo::IsUsing())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_1)) { m_operation = ImGuizmo::TRANSLATE; }
		if (ImGui::IsKeyPressed(ImGuiKey_3)) { m_operation = ImGuizmo::SCALE; }
	}

	// マップ対象は、MapEditor側の変更(Undo/ホットリロード/削除)に追従するため、
	// 編集中でない間は毎フレームデータから読み直す
	if (m_source == Source::Map && m_targetValid && !ImGui::IsAnyItemActive())
	{
		SelectMapObject(m_mapSelectedId);
	}

	ImGuiViewport* mainViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(
		ImVec2(mainViewport->Pos.x + mainViewport->Size.x + 20.0f, mainViewport->Pos.y + 40.0f),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(1000.0f, 700.0f), ImGuiCond_FirstUseEver);

	ImGui::Begin("Collider Editor Window");
	{
		ImGuiID dockId = ImGui::GetID("ColliderDockSpace");
		if (ImGui::DockBuilderGetNode(dockId) == nullptr)
		{
			SetupDockLayout(dockId, ImGui::GetContentRegionAvail());
		}
		ImGui::DockSpace(dockId, ImVec2(0, 0));
	}
	ImGui::End();

	DrawTargets();
	DrawInspector();
	DrawPreviewWindow();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 対象一覧ウィンドウ(Prefab / Map Objects)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ColliderEditor::DrawTargets()
{
	ImGui::Begin("Collider Targets");
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) { m_focused = true; }

	if (ImGui::BeginTabBar("ColliderSourceTabs"))
	{
		if (ImGui::BeginTabItem("Prefab"))
		{
			if (m_source != Source::Prefab)
			{
				m_source = Source::Prefab;
				SelectPrefabNode(m_prefabIndex);
			}
			DrawPrefabTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Map Objects"))
		{
			if (m_source != Source::Map)
			{
				m_source = Source::Map;
				SelectMapObject(m_mapSelectedId);
			}
			DrawMapTab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}

void ColliderEditor::DrawPrefabTab()
{
	ImGui::InputText("Path", m_prefabPathBuf, sizeof(m_prefabPathBuf));

	if (ImGui::Button("Load")) { LoadPrefab(m_prefabPathBuf); }
	ImGui::SameLine();
	ImGui::BeginDisabled(!m_prefabLoaded);
	if (ImGui::Button(m_prefabDirty ? "Save *" : "Save")) { SavePrefab(); }
	ImGui::EndDisabled();

	ImGui::Separator();

	if (!m_prefabLoaded)
	{
		ImGui::TextDisabled("Prefab(JSON)を読み込んでください");
		return;
	}

	for (int i = 0; i < (int)m_prefabNodes.size(); ++i)
	{
		const bool hasCollider = FindComponent(*m_prefabNodes[i].node, "Collider") != nullptr;
		std::string label = m_prefabNodes[i].label + (hasCollider ? "" : "  (Colliderなし)") + "##prefab" + std::to_string(i);

		if (ImGui::Selectable(label.c_str(), i == m_prefabIndex))
		{
			SelectPrefabNode(i);
		}
	}
}

void ColliderEditor::DrawMapTab()
{
	if (ImGui::Button("Save Map")) { MapEditor::Instance().SaveToCurrentPath(); }

	ImGui::Separator();

	for (auto& obj : MapEditor::Instance().WorkObjects())
	{
		const bool hasCollider = obj.FindComponent("Collider") != nullptr;
		std::string label = obj.data.name + (hasCollider ? "" : "  (Colliderなし)") + "##map" + std::to_string(obj.data.id);

		if (ImGui::Selectable(label.c_str(), m_mapSelectedId == obj.data.id))
		{
			SelectMapObject(obj.data.id);
		}
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Inspector(形状の一覧・追加/削除と、選択中形状の編集)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ColliderEditor::DrawInspector()
{
	ImGui::Begin("Collider Inspector");
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) { m_focused = true; }

	if (!m_targetValid)
	{
		ImGui::TextDisabled("対象を選択してください");
		ImGui::End();
		return;
	}

	ImGui::Text("Target : %s", m_targetLabel.c_str());

	if (!m_hasCollider)
	{
		if (ImGui::Button("Collider コンポーネントを追加"))
		{
			m_params = ColliderParams();
			m_selectedShape = -1;
			m_hasCollider = true;
			m_undoStack.clear();
			m_redoStack.clear();
			WriteBack();
		}
		ImGui::End();
		return;
	}

	// ---- Undo / Redo / 表示 ----
	ImGui::BeginDisabled(m_undoStack.empty());
	if (ImGui::Button("Undo (Ctrl+Z)")) { Undo(); }
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(m_redoStack.empty());
	if (ImGui::Button("Redo (Ctrl+Y)")) { Redo(); }
	ImGui::EndDisabled();

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput)
	{
		const bool ctrl = ImGui::GetIO().KeyCtrl;
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
		{
			if (ImGui::GetIO().KeyShift) { Redo(); }
			else { Undo(); }
		}
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) { Redo(); }
	}

	ImGui::Checkbox("Show Model", &m_showModel);
	if (!m_ancestors.empty())
	{
		ImGui::SameLine();
		ImGui::Checkbox("Show Parent", &m_showParent);
		if (m_showParent)
		{
			ImGui::SameLine();
			ImGui::Checkbox("Parent Colliders", &m_showParentColliders);
		}
	}
	if (ImGui::Button("Focus")) { m_previewTarget = ComputeFocus(); m_preview.ResetPan(); }

	// ---- ギズモ設定 ----
	ImGui::Checkbox("Show Gizmo", &m_showGizmo);
	if (m_showGizmo)
	{
		if (ImGui::RadioButton("Translate(1)", m_operation == ImGuizmo::TRANSLATE)) { m_operation = ImGuizmo::TRANSLATE; }
		ImGui::SameLine();
		if (ImGui::RadioButton("Scale(3)", m_operation == ImGuizmo::SCALE)) { m_operation = ImGuizmo::SCALE; }

		const bool capsuleSelected = m_selectedShape >= 0 && m_selectedShape < (int)m_params.shapes.size()
			&& ColliderShapeFromName(m_params.shapes[m_selectedShape].shape) == ColliderShape::Capsule;
		if (capsuleSelected && m_operation == ImGuizmo::TRANSLATE)
		{
			ImGui::RadioButton("Start", &m_capsuleHandle, 0);
			ImGui::SameLine();
			ImGui::RadioButton("End", &m_capsuleHandle, 1);
			ImGui::SameLine();
			ImGui::RadioButton("Both", &m_capsuleHandle, 2);
		}

		ImGui::Checkbox("Snap", &m_useSnap);
		if (m_useSnap)
		{
			ImGui::SameLine();
			ImGui::SetNextItemWidth(80.0f);
			ImGui::DragFloat("##SnapValue", &m_snapValue, 0.01f, 0.001f, 10.0f, "%.3f");
		}
	}

	ImGui::Separator();

	bool changed = false;

	// ---- コンポーネント全体の設定 ----
	{
		bool wire = m_params.wireFrame;
		if (ImGui::Checkbox("wireFrame (実行時にワイヤー表示)", &wire)) { PushUndo(); m_params.wireFrame = wire; changed = true; }

		bool ignoreParent = m_params.ignoreParent;
		if (ImGui::Checkbox("ignoreParent", &ignoreParent)) { PushUndo(); m_params.ignoreParent = ignoreParent; changed = true; }

		ImGui::Text("Body : %s", m_params.bodyShapeName.empty() ? "(なし)" : m_params.bodyShapeName.c_str());
	}

	ImGui::Separator();
	ImGui::Text("Shapes");

	// ---- 追加 / 複製 / 削除 ----
	ImGui::SetNextItemWidth(100.0f);
	ImGui::Combo("##AddType", &m_addType, kShapeNames, IM_ARRAYSIZE(kShapeNames));
	ImGui::SameLine();
	if (ImGui::Button("+ Add"))
	{
		PushUndo();
		ColliderShapeParams s;
		s.name = MakeUniqueName(kShapeNames[m_addType]);
		s.shape = kShapeNames[m_addType];
		m_params.shapes.push_back(s);
		m_selectedShape = (int)m_params.shapes.size() - 1;
		changed = true;
	}

	const bool hasSel = m_selectedShape >= 0 && m_selectedShape < (int)m_params.shapes.size();

	ImGui::SameLine();
	ImGui::BeginDisabled(!hasSel);
	if (ImGui::Button("Duplicate"))
	{
		PushUndo();
		ColliderShapeParams copy = m_params.shapes[m_selectedShape];
		copy.name = MakeUniqueName(copy.name);
		m_params.shapes.push_back(copy);
		m_selectedShape = (int)m_params.shapes.size() - 1;
		changed = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Remove"))
	{
		PushUndo();
		if (m_params.bodyShapeName == m_params.shapes[m_selectedShape].name) { m_params.bodyShapeName.clear(); }
		m_params.shapes.erase(m_params.shapes.begin() + m_selectedShape);
		ClampSelection();
		changed = true;
	}
	ImGui::EndDisabled();

	// ---- 形状リスト ----
	ImGui::BeginChild("ShapeList", ImVec2(0, 110), true);
	for (int i = 0; i < (int)m_params.shapes.size(); ++i)
	{
		const ColliderShapeParams& s = m_params.shapes[i];
		std::string label = s.name
			+ (s.name == m_params.bodyShapeName ? " [Body]" : "")
			+ " (" + s.shape + ")##shape" + std::to_string(i);

		if (ImGui::Selectable(label.c_str(), i == m_selectedShape)) { m_selectedShape = i; }
	}
	ImGui::EndChild();

	// ---- 選択中の形状 ----
	ImGui::Separator();
	if (m_selectedShape >= 0 && m_selectedShape < (int)m_params.shapes.size())
	{
		DrawShapeInspector(m_params.shapes[m_selectedShape], changed);
	}
	else
	{
		ImGui::TextDisabled("形状を選択してください");
	}

	if (changed) { WriteBack(); }

	ImGui::End();
}

void ColliderEditor::DrawShapeInspector(ColliderShapeParams& s, bool& changed)
{
	// 値を直接編集する系の共通処理(ドラッグ開始の瞬間にUndoを積む)
	auto drag3 = [&](const char* label, Math::Vector3& v, float speed)
		{
			if (ImGui::DragFloat3(label, &v.x, speed)) { changed = true; }
			if (ImGui::IsItemActivated()) { PushUndo(); }
		};
	auto check = [&](const char* label, bool& v)
		{
			bool nv = v;
			if (ImGui::Checkbox(label, &nv)) { PushUndo(); v = nv; changed = true; }
		};

	// ---- 名前(ColliderComponentは名前をキーにするため重複不可) ----
	{
		char buf[128];
		strcpy_s(buf, s.name.c_str());
		ImGui::InputText("Name", buf, sizeof(buf));
		if (ImGui::IsItemActivated()) { PushUndo(); }
		if (ImGui::IsItemEdited())
		{
			if (m_params.bodyShapeName == s.name) { m_params.bodyShapeName = buf; }
			s.name = buf;
			changed = true;
		}

		int dup = 0;
		for (auto& other : m_params.shapes) { if (other.name == s.name) { ++dup; } }
		if (dup > 1) { ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "名前が重複しています(実行時に上書きされます)"); }
	}

	// ---- 形状の種類 ----
	int shapeIdx = 2;	// 名前表(Sphere/Capsule/Box)のindexに直す
	switch (ColliderShapeFromName(s.shape))
	{
	case ColliderShape::Sphere:  shapeIdx = 0; break;
	case ColliderShape::Capsule: shapeIdx = 1; break;
	default:                     shapeIdx = 2; break;
	}
	if (ImGui::Combo("Shape", &shapeIdx, kShapeNames, IM_ARRAYSIZE(kShapeNames)))
	{
		PushUndo();
		s.shape = kShapeNames[shapeIdx];
		changed = true;
	}

	const ColliderShape type = ColliderShapeFromName(s.shape);

	// ---- 形状ごとの寸法 ----
	if (type == ColliderShape::Capsule)
	{
		drag3("Start", s.offset, 0.01f);
		drag3("End", s.capsuleEnd, 0.01f);
	}
	else
	{
		drag3("Offset", s.offset, 0.01f);
	}

	if (type != ColliderShape::Box)
	{
		if (ImGui::DragFloat("Radius", &s.radius, 0.005f, 0.01f, 100.0f)) { changed = true; }
		if (ImGui::IsItemActivated()) { PushUndo(); }
	}
	else
	{
		drag3("HalfExtents", s.halfExtents, 0.01f);
	}

	if (type == ColliderShape::Capsule)
	{
		// 足元の高さの目安(GroundSensorのfootOffsetと見比べる用)
		const float bottom = std::min(s.offset.y, s.capsuleEnd.y) - s.radius;
		ImGui::Text("下端Y(ローカル) : %.3f", bottom);

		if (ImGui::Button("Set as Body"))
		{
			PushUndo();
			m_params.bodyShapeName = s.name;
			changed = true;
		}
	}

	ImGui::Separator();

	// ---- フラグ ----
	check("enabled", s.enabled);
	check("isTrigger", s.isTrigger);
	check("isStatic", s.isStatic);
	check("wantsStayEvent", s.wantsStayEvent);

	ImGui::Separator();

	// ---- カテゴリ / 衝突マスク ----
	auto drawMask = [&](const char* title, std::vector<std::string>& names)
		{
			ColliderCategory mask = ColliderCategoryFromNames(names);
			ImGui::Text("%s", title);

			int i = 0;
			for (auto& kv : GetColliderCategoryFlags())
			{
				if (i++ % 4 != 0) { ImGui::SameLine(); }

				bool on = Any(mask & kv.second);
				std::string label = std::string(kv.first) + "##" + title;
				if (ImGui::Checkbox(label.c_str(), &on))
				{
					PushUndo();
					if (on) { mask |= kv.second; }
					else { mask = mask & static_cast<ColliderCategory>(~static_cast<uint32_t>(kv.second)); }
					names = ColliderCategoryToNames(mask);
					changed = true;
				}
			}
		};

	drawMask("categoryMask", s.categoryMask);

	check("useDefaultCollideMask", s.useDefaultCollideMask);
	if (!s.useDefaultCollideMask)
	{
		drawMask("collideMask", s.collideMask);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// プレビューウィンドウ
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ColliderEditor::DrawPreviewWindow()
{
	m_preview.DrawWindow("Collider Preview", ImGuiWindowFlags_NoMove, [this]()
		{
			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) { m_focused = true; }

			// ギズモはプレビュー画像と同じウィンドウ内で描く(ImGuizmo::SetDrawlist()が現在のウィンドウを使う為)
			DrawGizmo();

			ImGui::SetCursorPos(ImVec2(10, 10));
			if (m_targetValid)
			{
				ImGui::Text("%s", m_targetLabel.c_str());
				ImGui::SetCursorPosX(10);
				ImGui::TextDisabled("右ドラッグ:回転 / 右ホールド+WASDQE:移動 / 中ドラッグ:平行移動 / ホイール:ズーム / F:戻す");
			}
			else
			{
				ImGui::TextDisabled("対象が選択されていません");
			}
		});
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ギズモ操作
//	Translate: Sphere/Boxはoffset、Capsuleは始点/終点/両方を移動
//	Scale    : Boxはhalf extents、Sphere/Capsuleは半径
//	形状はモデルのスケールだけを受けるローカル座標なので、ワールド⇔ローカルの変換はスケールの除算で済む
//	※ImGuizmo::BeginFrame()はMapEditor::Update()が毎フレーム呼ぶものに依存している
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ColliderEditor::DrawGizmo()
{
	const bool canUse = m_showGizmo && m_targetValid && m_hasCollider
		&& m_selectedShape >= 0 && m_selectedShape < (int)m_params.shapes.size()
		&& m_preview.IsReady();
	if (!canUse) { m_gizmoWasUsing = false; return; }

	ColliderShapeParams& s = m_params.shapes[m_selectedShape];
	const ColliderShape type = ColliderShapeFromName(s.shape);

	// 形状が置かれているワールド空間(親の回転・スケールを含む)
	const Math::Matrix world = m_targetWorld;
	const Math::Matrix invWorld = world.Invert();

	Math::Matrix decomposeSrc = world;
	Math::Vector3 worldScale, worldTrans;
	Math::Quaternion worldRot;
	decomposeSrc.Decompose(worldScale, worldRot, worldTrans);

	const Math::Vector3 absScale(std::abs(worldScale.x), std::abs(worldScale.y), std::abs(worldScale.z));
	const float maxScale = std::max({ absScale.x, absScale.y, absScale.z, 0.0001f });

	// ギズモを置くローカル位置
	Math::Vector3 localPos = s.offset;
	if (type == ColliderShape::Capsule)
	{
		if (m_operation == ImGuizmo::SCALE || m_capsuleHandle == 2) { localPos = (s.offset + s.capsuleEnd) * 0.5f; }
		else if (m_capsuleHandle == 1) { localPos = s.capsuleEnd; }
	}
	const Math::Vector3 worldPos = Math::Vector3::Transform(localPos, world);

	// Scale時は、現在の寸法(ワールド換算)をギズモの行列スケールとして渡す
	Math::Vector3 gizmoScale(1, 1, 1);
	if (m_operation == ImGuizmo::SCALE)
	{
		if (type == ColliderShape::Box)
		{
			gizmoScale = Math::Vector3(
				std::max(s.halfExtents.x * absScale.x, 0.01f),
				std::max(s.halfExtents.y * absScale.y, 0.01f),
				std::max(s.halfExtents.z * absScale.z, 0.01f));
		}
		else
		{
			const float r = std::max(s.radius * maxScale, 0.01f);
			gizmoScale = Math::Vector3(r, r, r);
		}
	}

	// 回転は対象の向きに合わせる(Scaleギズモの軸がBoxの軸と一致する)。Translateはワールド軸のまま操作する
	Math::Matrix matrix = Math::Matrix::CreateScale(gizmoScale)
		* Math::Matrix::CreateFromQuaternion(worldRot)
		* Math::Matrix::CreateTranslation(worldPos);

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();
	const ImVec2& rectPos = m_preview.GetScreenPos();
	const ImVec2& rectSize = m_preview.GetScreenSize();
	ImGuizmo::SetRect(rectPos.x, rectPos.y, rectSize.x, rectSize.y);

	const Math::Matrix view = m_preview.GetViewMatrix(m_previewTarget);
	const Math::Matrix proj = m_preview.GetProjMatrix();
	const float snap[3] = { m_snapValue, m_snapValue, m_snapValue };

	ImGuizmo::Manipulate(
		reinterpret_cast<const float*>(&view),
		reinterpret_cast<const float*>(&proj),
		m_operation, ImGuizmo::WORLD, reinterpret_cast<float*>(&matrix),
		nullptr,
		m_useSnap ? snap : nullptr);

	const bool isUsing = ImGuizmo::IsUsing();

	// ドラッグ開始の瞬間(まだm_paramsを書き換える前)にUndoを1回積む
	if (isUsing && !m_gizmoWasUsing) { PushUndo(); }

	if (isUsing)
	{
		Math::Vector3 newScale, newPos;
		Math::Quaternion newRot;
		matrix.Decompose(newScale, newRot, newPos);

		if (m_operation == ImGuizmo::TRANSLATE)
		{
			const Math::Vector3 newLocal = Math::Vector3::Transform(newPos, invWorld);

			if (type == ColliderShape::Capsule && m_capsuleHandle == 2)
			{
				const Math::Vector3 delta = newLocal - localPos;
				s.offset += delta;
				s.capsuleEnd += delta;
			}
			else if (type == ColliderShape::Capsule && m_capsuleHandle == 1)
			{
				s.capsuleEnd = newLocal;
			}
			else
			{
				s.offset = newLocal;
			}
		}
		else if (type == ColliderShape::Box)
		{
			s.halfExtents = Math::Vector3(
				std::max(newScale.x / std::max(absScale.x, 0.0001f), 0.01f),
				std::max(newScale.y / std::max(absScale.y, 0.0001f), 0.01f),
				std::max(newScale.z / std::max(absScale.z, 0.0001f), 0.01f));
		}
		else
		{
			// 3軸のうち最も動いた軸の値を半径に採用する
			const float dx = std::abs(newScale.x - gizmoScale.x);
			const float dy = std::abs(newScale.y - gizmoScale.y);
			const float dz = std::abs(newScale.z - gizmoScale.z);
			float v = newScale.x;
			if (dy > dx && dy >= dz) { v = newScale.y; }
			else if (dz > dx && dz > dy) { v = newScale.z; }
			s.radius = std::max(v / maxScale, 0.01f);
		}

		WriteBack();
	}

	m_gizmoWasUsing = isUsing;
}

// ///// /////
// 専用カメラ・専用バッファへ、モデルとコライダーを描画する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ColliderEditor::RenderPreviewViewport()
{
	if (!m_preview.IsReady()) return;

	m_preview.Render(m_previewTarget, [this]()
		{
			// 描くモデル(自分 + 親)とそのワールド行列
			std::vector<std::pair<KdModelWork*, Math::Matrix>> models;
			if (m_showModel)
			{
				if (KdModelWork* w = GetPreviewModel()) { models.emplace_back(w, m_targetWorld); }
			}
			if (m_showParent)
			{
				for (AncestorPreview& a : m_ancestors) { models.emplace_back(&a.model, a.world); }
			}

			auto drawAll = [&]()
				{
					for (auto& m : models)
					{
						KdModelWork* work = m.first;
						if (!work->IsEnable()) continue;

						if (work->NeedCalcNodeMatrices()) { work->CalcNodeMatrices(); }
						KdShaderManager::Instance().m_StandardShader.DrawModel(*work, m.second);
					}
				};

			if (!models.empty())
			{
				auto& shader = KdShaderManager::Instance().m_StandardShader;
				shader.BeginGenerateDepthMapFromLight();
				drawAll();
				shader.EndGenerateDepthMapFromLight();
				shader.BeginLit();
				drawAll();
				shader.EndLit();
			}

			DrawShapeWires();
		});
}

// 選択中は黄、無効は灰、トリガーは赤、それ以外は緑。親のコライダーは青
void ColliderEditor::DrawShapeWires()
{
	if (m_showParent && m_showParentColliders)
	{
		const Math::Color parentCol(0.35f, 0.6f, 0.9f, 1.0f);
		for (const AncestorPreview& a : m_ancestors)
		{
			if (!a.hasCollider) continue;
			for (const ColliderShapeParams& s : a.collider.shapes)
			{
				if (!s.enabled) continue;
				WireFrameComponent::AddShapeWire(m_wire, ToPreviewEntry(s), a.world, parentCol);
			}
		}
	}

	if (m_targetValid && m_hasCollider)
	{
		for (int i = 0; i < (int)m_params.shapes.size(); ++i)
		{
			const ColliderShapeParams& s = m_params.shapes[i];

			Math::Color col = s.isTrigger ? kRedColor : kGreenColor;
			if (!s.enabled) { col = Math::Color(0.45f, 0.45f, 0.45f, 1.0f); }
			if (i == m_selectedShape) { col = Math::Color(1.0f, 0.9f, 0.1f, 1.0f); }

			WireFrameComponent::AddShapeWire(m_wire, ToPreviewEntry(s), m_targetWorld, col);
		}
	}

	m_wire.Draw();
}

// 全形状の中心(ワールド座標)の平均をプレビューの注視点にする
Math::Vector3 ColliderEditor::ComputeFocus() const
{
	if (m_params.shapes.empty()) return Math::Vector3::Zero;

	Math::Vector3 sum = Math::Vector3::Zero;
	for (const ColliderShapeParams& s : m_params.shapes)
	{
		Math::Vector3 c = s.offset;
		if (ColliderShapeFromName(s.shape) == ColliderShape::Capsule) { c = (s.offset + s.capsuleEnd) * 0.5f; }
		sum += c;
	}
	sum /= (float)m_params.shapes.size();

	return Math::Vector3::Transform(sum, m_targetWorld);
}

KdModelWork* ColliderEditor::GetPreviewModel()
{
	if (m_source == Source::Map)
	{
		MapObject* obj = FindMapObject(m_mapSelectedId);
		return obj ? &obj->modelWork : nullptr;
	}
	return &m_modelWork;
}

void ColliderEditor::LoadPreviewModel(const std::string& path)
{
	if (path == m_modelPath && (path.empty() || m_modelWork.IsEnable())) return;

	m_modelPath = path;
	m_modelWork = LoadModelWork(path);
}

KdModelWork ColliderEditor::LoadModelWork(const std::string& path)
{
	KdModelWork work;
	if (path.empty()) return work;

	std::shared_ptr<KdModelData> data = KdAssets::Instance().m_modeldatas.GetData(path);
	if (!data)
	{
		KdDebugGUI::Instance().AddLog("ColliderEditor: モデル読み込み失敗 %s\n", path.c_str());
		return work;
	}
	work.SetModelData(data);
	return work;
}

// ///// /////
// Prefabの読み込み・保存
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool ColliderEditor::LoadPrefab(const std::string& path)
{
	nlohmann::ordered_json root;
	if (!JsonLoader::Load(path, root))
	{
		KdDebugGUI::Instance().AddLog("ColliderEditor: Prefab読み込み失敗 %s\n", path.c_str());
		return false;
	}

	m_prefabRoot = std::move(root);
	m_prefabNodes.clear();
	CollectPrefabNodes(m_prefabRoot, "", -1);

	m_prefabLoaded = true;
	m_prefabDirty = false;

	SelectPrefabNode(m_prefabNodes.empty() ? -1 : 0);

	KdDebugGUI::Instance().AddLog("ColliderEditor: 読み込みました %s\n", path.c_str());
	return true;
}

// 保存はColliderの変更だけをJSONへ反映済みの状態でそのまま書き出す(ordered_jsonなのでキー順は保たれる)
void ColliderEditor::SavePrefab()
{
	if (!m_prefabLoaded) return;

	if (!JsonLoader::Save(m_prefabPathBuf, m_prefabRoot))
	{
		KdDebugGUI::Instance().AddLog("ColliderEditor: Prefab保存失敗 %s\n", m_prefabPathBuf);
		return;
	}

	m_prefabDirty = false;
	KdDebugGUI::Instance().AddLog("ColliderEditor: 保存しました %s\n", m_prefabPathBuf);

	// 現在のシーンを再生成させる
	GLOBALEVENT->Publish(Events::ReloadingScene());
}

void ColliderEditor::CollectPrefabNodes(nlohmann::ordered_json& node, const std::string& prefix, int parent)
{
	const std::string label = prefix + node.value("name", std::string("GameObject"));

	PrefabNode pn;
	pn.label = label;
	pn.node = &node;
	pn.parent = parent;
	m_prefabNodes.push_back(pn);
	const int myIndex = (int)m_prefabNodes.size() - 1;

	if (node.contains("children") && node["children"].is_array())
	{
		for (auto& child : node["children"])
		{
			CollectPrefabNodes(child, label + "/", myIndex);
		}
	}
}

Math::Matrix ColliderEditor::ComputeNodeWorld(int index)
{
	const PrefabNode& pn = m_prefabNodes[index];
	if (pn.parent < 0) { return ReadNodeLocalMatrix(*pn.node, true); }

	const Math::Matrix parentWorld = ComputeNodeWorld(pn.parent);

	// AttachToBoneの子は、自分のpos/rotationではなくボーンの位置が優先される(実行時と同じ)
	if (nlohmann::ordered_json* attach = FindComponent(*pn.node, "AttachToBone"))
	{
		Math::Matrix attached;
		if (TryComputeBoneAttachedWorld(pn.parent, parentWorld, *pn.node, *attach, attached)) { return attached; }
	}

	return ReadNodeLocalMatrix(*pn.node, false) * parentWorld;
}

// 実行時: ワールド位置 = ボーン位置 + ボーン回転で回した(position * 親scale)、回転 = rotation * ボーン回転、
// スケール = 自分のscale * 親scale。ボーンのscaleは使わない。
bool ColliderEditor::TryComputeBoneAttachedWorld(int parentIndex, const Math::Matrix& parentWorld,
	const nlohmann::ordered_json& node, const nlohmann::ordered_json& attach, Math::Matrix& out)
{
	const nlohmann::json params = attach.contains("params") ? ToJson(attach["params"]) : nlohmann::json::object();

	const std::string bone = params.value("bone", std::string());
	if (bone.empty()) return false;

	nlohmann::ordered_json& parentNode = *m_prefabNodes[parentIndex].node;

	// ボーンは親のモデル(スケルトン)から取る
	KdModelWork parentModel = LoadModelWork(ReadModelPath(parentNode));
	Math::Matrix boneInModel;
	if (!TryGetBoneModelMatrix(parentModel, bone, boneInModel))
	{
		KdDebugGUI::Instance().AddLog("ColliderEditor: ボーン取得失敗 %s (親モデルを確認してください)\n", bone.c_str());
		return false;
	}

	Math::Vector3 boneScale, bonePos;
	Math::Quaternion boneRot;
	Math::Matrix boneWorld = boneInModel * parentWorld;
	if (!boneWorld.Decompose(boneScale, boneRot, bonePos)) return false;

	// AttachToBoneのparams(position / rotationEulerDeg)
	Math::Vector3 attachPos(0, 0, 0);
	Math::Quaternion attachRot = Math::Quaternion::Identity;
	try
	{
		if (params.contains("position")) { attachPos = params["position"].get<Math::Vector3>(); }
		if (params.contains("rotationEulerDeg") && params["rotationEulerDeg"].is_array() && params["rotationEulerDeg"].size() >= 3)
		{
			const auto& e = params["rotationEulerDeg"];
			attachRot = Math::Quaternion::CreateFromYawPitchRoll(
				DirectX::XMConvertToRadians(e[1].get<float>()),
				DirectX::XMConvertToRadians(e[0].get<float>()),
				DirectX::XMConvertToRadians(e[2].get<float>()));
		}
	}
	catch (const nlohmann::json::exception&) {}

	const Math::Vector3 ownerScale = ReadNodeScale(parentNode);
	const Math::Vector3 selfScale = ReadNodeScale(node) * ownerScale;
	const Math::Vector3 rotatedOffset = Math::Vector3::Transform(attachPos * ownerScale, boneRot);

	out = Math::Matrix::CreateScale(selfScale)
		* Math::Matrix::CreateFromQuaternion(attachRot * boneRot)
		* Math::Matrix::CreateTranslation(bonePos + rotatedOffset);
	return true;
}

nlohmann::ordered_json* ColliderEditor::CurrentPrefabNode()
{
	if (m_prefabIndex < 0 || m_prefabIndex >= (int)m_prefabNodes.size()) return nullptr;
	return m_prefabNodes[m_prefabIndex].node;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 対象の選択(JSONからm_paramsへ読み込む)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ColliderEditor::ClearTarget()
{
	m_targetValid = false;
	m_hasCollider = false;
	m_ancestors.clear();
	m_targetWorld = Math::Matrix::Identity;
	m_params = ColliderParams();
	m_selectedShape = -1;
	m_undoStack.clear();
	m_redoStack.clear();
}

void ColliderEditor::ReadParams(const nlohmann::json& paramsJson)
{
	try
	{
		m_params = paramsJson.get<ColliderParams>();
	}
	catch (const nlohmann::json::exception&)
	{
		m_params = ColliderParams();
		KdDebugGUI::Instance().AddLog("ColliderEditor: Colliderのparams解釈に失敗(%s)\n", m_targetLabel.c_str());
	}
}

void ColliderEditor::SelectPrefabNode(int index)
{
	m_prefabIndex = index;

	nlohmann::ordered_json* node = CurrentPrefabNode();
	if (!node) { ClearTarget(); return; }

	m_targetLabel = m_prefabNodes[index].label;

	m_params = ColliderParams();
	nlohmann::ordered_json* comp = FindComponent(*node, "Collider");
	m_hasCollider = (comp != nullptr);
	if (comp && comp->contains("params")) { ReadParams(ToJson((*comp)["params"])); }

	// 自分のモデルとワールド行列
	LoadPreviewModel(ReadModelPath(*node));
	m_targetWorld = ComputeNodeWorld(index);

	// 位置合わせ用に、親(ルートまで)のモデルとコライダーを読み込む
	m_ancestors.clear();
	for (int p = m_prefabNodes[index].parent; p >= 0; p = m_prefabNodes[p].parent)
	{
		nlohmann::ordered_json& pn = *m_prefabNodes[p].node;

		AncestorPreview a;
		a.model = LoadModelWork(ReadModelPath(pn));
		a.world = ComputeNodeWorld(p);

		if (nlohmann::ordered_json* pc = FindComponent(pn, "Collider"))
		{
			if (pc->contains("params"))
			{
				try
				{
					a.collider = ToJson((*pc)["params"]).get<ColliderParams>();
					a.hasCollider = true;
				}
				catch (const nlohmann::json::exception&) {}
			}
		}
		m_ancestors.push_back(std::move(a));
	}

	m_undoStack.clear();
	m_redoStack.clear();
	m_selectedShape = m_params.shapes.empty() ? -1 : 0;
	m_previewTarget = ComputeFocus();
	m_preview.ResetPan();
	m_targetValid = true;
}

void ColliderEditor::SelectMapObject(ObjectId id)
{
	MapObject* obj = FindMapObject(id);
	if (!obj) { m_mapSelectedId = kInvalidObjectId; ClearTarget(); return; }

	const bool sameTarget = m_targetValid && m_source == Source::Map && id == m_mapSelectedId;
	m_mapSelectedId = id;
	m_targetLabel = obj->data.name;

	m_params = ColliderParams();
	ComponentEntry* comp = FindMapComponent(*obj, "Collider");
	m_hasCollider = (comp != nullptr);
	if (comp) { ReadParams(comp->params); }

	m_targetWorld = Math::Matrix::CreateScale(obj->data.scale);
	m_ancestors.clear();

	if (!sameTarget)
	{
		// 別のオブジェクトへ切り替わった時だけ、履歴と注視点を作り直す
		m_undoStack.clear();
		m_redoStack.clear();
		m_selectedShape = m_params.shapes.empty() ? -1 : 0;
		m_previewTarget = ComputeFocus();
		m_preview.ResetPan();
	}

	ClampSelection();
	m_targetValid = true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// m_paramsを現在の対象のJSONへ書き戻す
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ColliderEditor::WriteBack()
{
	if (m_source == Source::Prefab)
	{
		nlohmann::ordered_json* node = CurrentPrefabNode();
		if (!node) return;

		nlohmann::ordered_json params = m_params;	// メンバ宣言順で書き出される
		if (nlohmann::ordered_json* comp = FindComponent(*node, "Collider"))
		{
			(*comp)["params"] = params;
		}
		else
		{
			nlohmann::ordered_json entry;
			entry["type"] = "Collider";
			entry["params"] = params;
			(*node)["components"].push_back(entry);
		}

		m_prefabDirty = true;
	}
	else
	{
		MapObject* obj = FindMapObject(m_mapSelectedId);
		if (!obj) return;

		nlohmann::json params = m_params;
		if (ComponentEntry* comp = FindMapComponent(*obj, "Collider"))
		{
			comp->params = params;
		}
		else
		{
			ComponentEntry entry;
			entry.type = "Collider";
			entry.params = params;
			obj->data.components.push_back(entry);
		}
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Undo / Redo(ColliderParams全体のスナップショット)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ColliderEditor::PushUndo()
{
	m_undoStack.push_back(m_params);
	if (m_undoStack.size() > kMaxUndoDepth) { m_undoStack.erase(m_undoStack.begin()); }
	m_redoStack.clear();
}

void ColliderEditor::Undo()
{
	if (m_undoStack.empty()) return;

	m_redoStack.push_back(m_params);
	m_params = std::move(m_undoStack.back());
	m_undoStack.pop_back();

	ClampSelection();
	WriteBack();
}

void ColliderEditor::Redo()
{
	if (m_redoStack.empty()) return;

	m_undoStack.push_back(m_params);
	m_params = std::move(m_redoStack.back());
	m_redoStack.pop_back();

	ClampSelection();
	WriteBack();
}

void ColliderEditor::ClampSelection()
{
	if (m_params.shapes.empty()) { m_selectedShape = -1; return; }
	m_selectedShape = std::clamp(m_selectedShape, 0, (int)m_params.shapes.size() - 1);
}

std::string ColliderEditor::MakeUniqueName(const std::string& base) const
{
	auto exists = [this](const std::string& n)
		{
			for (auto& s : m_params.shapes) { if (s.name == n) return true; }
			return false;
		};

	if (!exists(base)) return base;

	for (int i = 2;; ++i)
	{
		std::string candidate = base + std::to_string(i);
		if (!exists(candidate)) return candidate;
	}
}