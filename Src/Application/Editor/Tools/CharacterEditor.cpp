#include "Application/main.h"

#include "CharacterEditor.h"
#include "../Common/ComponentInspector.h"

#include "imgui_internal.h"

#include <algorithm>
#include <filesystem>

//=====================================================
// ドックレイアウト(左:一覧 / 中央:階層 / 右:Inspector / 下:メニュー)
//=====================================================
static void SetupCharacterDockLayout(ImGuiID dockspaceId, const ImVec2& size)
{
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceId, size);

	ImGuiID center = dockspaceId;
	ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.22f, nullptr, &center);
	ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.18f, nullptr, &center);
	ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.60f, nullptr, &center);

	ImGui::DockBuilderDockWindow("Characters", left);
	ImGui::DockBuilderDockWindow("Char Hierarchy", center);
	ImGui::DockBuilderDockWindow("Char Inspector", right);
	ImGui::DockBuilderDockWindow("Character Editor", bottom);

	ImGui::DockBuilderFinish(dockspaceId);
}

//=====================================================
// 毎フレーム更新
//=====================================================
void CharacterEditor::Update()
{
	m_focusedNow = false;

	m_doc.Tick(Application::Instance().GetDeltaTime());

	if (m_focused && !ImGui::GetIO().WantTextInput)
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
		{
			if (io.KeyShift) { Redo(); } else { Undo(); }
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) { Redo(); }
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) { Save(m_filePathBuf); }
	}

	ImGuiViewport* mainViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(
		ImVec2(mainViewport->Pos.x + mainViewport->Size.x + 20.0f, mainViewport->Pos.y + 40.0f),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(1000.0f, 700.0f), ImGuiCond_FirstUseEver);

	ImGui::Begin("Character Editor Window");
	{
		ImGuiID dockId = ImGui::GetID("CharacterDockSpace");
		if (ImGui::DockBuilderGetNode(dockId) == nullptr)
		{
			SetupCharacterDockLayout(dockId, ImGui::GetContentRegionAvail());
		}
		ImGui::DockSpace(dockId, ImVec2(0, 0));
	}
	ImGui::End();

	DrawMainMenu();
	DrawCharacterList();
	DrawHierarchy();
	DrawInspector();

	m_focused = m_focusedNow;
}

//=====================================================
// メニュー(保存/読込/Undo)
//=====================================================
void CharacterEditor::DrawMainMenu()
{
	ImGui::Begin("Character Editor");
	m_focusedNow |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	if (m_doc.Path() != m_lastDocPath)
	{
		m_lastDocPath = m_doc.Path();
		strncpy_s(m_filePathBuf, m_lastDocPath.c_str(), _TRUNCATE);
	}
	ImGui::InputText("Path", m_filePathBuf, sizeof(m_filePathBuf));

	if (ImGui::Button("Save (Ctrl+S)")) { Save(m_filePathBuf); }
	ImGui::SameLine();
	if (ImGui::Button("Load")) { RequestOpen(m_filePathBuf); }
	ImGui::SameLine();
	if (ImGui::Button("New")) { RequestNew(); }
	ImGui::SameLine();
	ImGui::Checkbox("Auto Reload", &m_doc.Watcher().autoReload);

	ImGui::BeginDisabled(!m_doc.CanUndo());
	if (ImGui::Button("Undo (Ctrl+Z)")) { Undo(); }
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!m_doc.CanRedo());
	if (ImGui::Button("Redo (Ctrl+Y)")) { Redo(); }
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (m_doc.IsDirty()) { ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "* 未保存の変更あり"); }

	// 未保存変更の確認
	if (m_openPopupRequested)
	{
		ImGui::OpenPopup("Unsaved Changes");
		m_openPopupRequested = false;
	}
	if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("未保存の変更があります。破棄して続行しますか？");

		auto proceed = [this]()
		{
			if (m_pendingIsNew) { NewFile(); } else { OpenFile(m_pendingPath); }
			ImGui::CloseCurrentPopup();
		};

		if (ImGui::Button("Save & Continue"))
		{
			Save(m_filePathBuf);
			proceed();
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard & Continue")) { proceed(); }
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}

	ImGui::End();
}

//=====================================================
// キャラクター一覧(フォルダ内で "components" 配列を持つJSON)
//=====================================================
void CharacterEditor::RefreshList()
{
	namespace fs = std::filesystem;

	m_list.clear();
	m_listLoaded = true;

	std::error_code ec;
	for (fs::directory_iterator it(m_listDirBuf, ec), end; !ec && it != end; it.increment(ec))
	{
		if (!it->is_regular_file() || it->path().extension() != ".json") continue;

		const std::string path = it->path().generic_string();
		nlohmann::json j;
		if (!JsonLoader::Load(path, j)) continue;
		if (!j.is_object() || !j.contains("components") || !j["components"].is_array()) continue;

		m_list.push_back({ it->path().stem().string(), path });
	}

	std::sort(m_list.begin(), m_list.end(),
		[](const ListEntry& a, const ListEntry& b) { return a.name < b.name; });
}

void CharacterEditor::DrawCharacterList()
{
	ImGui::Begin("Characters");
	m_focusedNow |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	ImGui::InputText("Dir", m_listDirBuf, sizeof(m_listDirBuf));
	if (ImGui::Button("Refresh") || !m_listLoaded) { RefreshList(); }

	ImGui::Separator();

	if (m_list.empty()) { ImGui::TextDisabled("キャラクターJSONが見つかりません"); }

	for (const ListEntry& e : m_list)
	{
		const bool isCurrent = (e.path == m_doc.Path());
		if (ImGui::Selectable(e.name.c_str(), isCurrent)) { RequestOpen(e.path); }
	}

	ImGui::End();
}

//=====================================================
// 階層ウィンドウ
//=====================================================
void CharacterEditor::DrawNodeTree(CharacterNode& node)
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth
		| ImGuiTreeNodeFlags_DefaultOpen;
	if (node.children.empty()) { flags |= ImGuiTreeNodeFlags_Leaf; }
	if (node.id == m_doc.SelectedId()) { flags |= ImGuiTreeNodeFlags_Selected; }

	const std::string label = node.name + "##" + std::to_string(node.id);
	const bool open = ImGui::TreeNodeEx(label.c_str(), flags);

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) { m_doc.SetSelected(node.id); }

	// ルート以外はドラッグで親を付け替えられる
	if (node.id != m_doc.File().root.id && ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("CHAR_NODE", &node.id, sizeof(NodeId));
		ImGui::TextUnformatted(node.name.c_str());
		ImGui::EndDragDropSource();
	}
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("CHAR_NODE"))
		{
			m_pendingMoveSrc = *static_cast<const NodeId*>(p->Data);
			m_pendingMoveDst = node.id;
		}
		ImGui::EndDragDropTarget();
	}

	if (open)
	{
		for (auto& c : node.children) { DrawNodeTree(c); }
		ImGui::TreePop();
	}
}

void CharacterEditor::DrawHierarchy()
{
	ImGui::Begin("Char Hierarchy");
	m_focusedNow |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	const bool isRoot = (m_doc.SelectedId() == m_doc.File().root.id);

	if (ImGui::Button("+ Child")) { AddChild(); }
	ImGui::SameLine();
	ImGui::BeginDisabled(isRoot || !FindSelected());
	if (ImGui::Button("Duplicate")) { DuplicateSelected(); }
	ImGui::SameLine();
	if (ImGui::Button("- Remove")) { RemoveSelected(); }
	ImGui::SameLine();
	if (ImGui::Button("Up")) { MoveSelected(-1); }
	ImGui::SameLine();
	if (ImGui::Button("Down")) { MoveSelected(+1); }
	ImGui::EndDisabled();

	ImGui::Separator();

	DrawNodeTree(m_doc.File().root);

	// 描画中は構造を変えず、ここでまとめて反映する
	if (m_pendingMoveSrc != kInvalidNodeId)
	{
		MoveNode(m_pendingMoveSrc, m_pendingMoveDst);
		m_pendingMoveSrc = m_pendingMoveDst = kInvalidNodeId;
	}

	ImGui::End();
}

//=====================================================
// インスペクター
//=====================================================
void CharacterEditor::DrawInspector()
{
	ImGui::Begin("Char Inspector");
	m_focusedNow |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	CharacterNode* node = FindSelected();
	if (!node)
	{
		ImGui::TextDisabled("ノードが選択されていません");
		ImGui::End();
		return;
	}

	char nameBuf[128];
	strncpy_s(nameBuf, node->name.c_str(), _TRUNCATE);
	ImGui::InputText("Name", nameBuf, sizeof(nameBuf));
	if (ImGui::IsItemActivated()) { PushUndo(); }
	if (ImGui::IsItemEdited()) { node->name = nameBuf; }

	if (ImGui::TreeNode("Transform (read only)"))
	{
		ImGui::Text("pos      : %.3f, %.3f, %.3f", node->pos.x, node->pos.y, node->pos.z);
		ImGui::Text("rotation : %.3f, %.3f, %.3f, %.3f", node->rotation.x, node->rotation.y, node->rotation.z, node->rotation.w);
		ImGui::Text("scale    : %.3f, %.3f, %.3f", node->scale.x, node->scale.y, node->scale.z);
		ImGui::TreePop();
	}

	ImGui::Separator();
	ComponentInspector::DrawList(
		node->components,
		[this]() { PushUndo(); },
		[](const std::string&) {});

	ImGui::End();
}

//=====================================================
// ノード操作
//=====================================================
void CharacterEditor::AddChild()
{
	CharacterNode* parent = FindSelected();
	if (!parent) { parent = &m_doc.File().root; }

	PushUndo();

	CharacterNode child;
	child.id = m_doc.File().nextId++;
	child.name = "Child" + std::to_string(child.id);
	const NodeId newId = child.id;
	parent->children.push_back(std::move(child));
	m_doc.SetSelected(newId);
}

void CharacterEditor::RemoveSelected()
{
	if (m_doc.SelectedId() == m_doc.File().root.id) return;

	CharacterNode* parent = m_doc.FindParent(m_doc.SelectedId());
	if (!parent) return;

	PushUndo();

	const NodeId id = m_doc.SelectedId();
	parent->children.erase(
		std::remove_if(parent->children.begin(), parent->children.end(),
			[id](const CharacterNode& c) { return c.id == id; }),
		parent->children.end());

	m_doc.SetSelected(kInvalidNodeId);
}

void CharacterEditor::DuplicateSelected()
{
	const NodeId selected = m_doc.SelectedId();
	if (selected == m_doc.File().root.id) return;

	CharacterNode* src = FindSelected();
	CharacterNode* parent = m_doc.FindParent(selected);
	if (!src || !parent) return;

	PushUndo();

	CharacterNode copy = *src;
	copy.name += "_copy";
	ReassignNodeIds(copy, m_doc.File().nextId);
	const NodeId newId = copy.id;

	// insertで参照が無効になるので、位置を先に求めてから挿入する
	auto it = std::find_if(parent->children.begin(), parent->children.end(),
		[selected](const CharacterNode& c) { return c.id == selected; });
	parent->children.insert(it + 1, std::move(copy));
	m_doc.SetSelected(newId);
}

void CharacterEditor::MoveSelected(int delta)
{
	const NodeId selected = m_doc.SelectedId();
	CharacterNode* parent = m_doc.FindParent(selected);
	if (!parent) return;

	auto& v = parent->children;
	auto it = std::find_if(v.begin(), v.end(),
		[selected](const CharacterNode& c) { return c.id == selected; });
	if (it == v.end()) return;

	const int idx = (int)(it - v.begin());
	const int dst = idx + delta;
	if (dst < 0 || dst >= (int)v.size()) return;

	PushUndo();
	std::swap(v[idx], v[dst]);
}

// srcをnewParentの末尾の子へ付け替える(自分自身/自分の子孫への移動は無視)
void CharacterEditor::MoveNode(NodeId src, NodeId newParent)
{
	if (src == m_doc.File().root.id || src == newParent) return;

	CharacterNode* s = m_doc.FindNode(src);
	CharacterNode* oldParent = m_doc.FindParent(src);
	if (!s || !oldParent) return;
	if (FindNodeById(*s, newParent)) return;	// 子孫への移動は循環になる

	PushUndo();

	CharacterNode moved = std::move(*s);
	oldParent->children.erase(
		std::remove_if(oldParent->children.begin(), oldParent->children.end(),
			[src](const CharacterNode& c) { return c.id == src; }),
		oldParent->children.end());

	// erase後は参照が無効なので、親を探し直す
	if (CharacterNode* np = m_doc.FindNode(newParent))
	{
		np->children.push_back(std::move(moved));
	}
}

//=====================================================
// ファイル操作(実体は共有のCharacterDocument)
//=====================================================
void CharacterEditor::RequestOpen(const std::string& path)
{
	if (!m_doc.IsDirtyNow()) { OpenFile(path); return; }

	m_pendingPath = path;
	m_pendingIsNew = false;
	m_openPopupRequested = true;
}

void CharacterEditor::RequestNew()
{
	if (!m_doc.IsDirtyNow()) { NewFile(); return; }

	m_pendingIsNew = true;
	m_openPopupRequested = true;
}

void CharacterEditor::OpenFile(const std::string& path)
{
	m_doc.Open(path);
}

void CharacterEditor::NewFile()
{
	m_doc.New();
}

void CharacterEditor::Save(const std::string& path)
{
	if (!m_doc.SaveAs(path)) return;

	m_listLoaded = false;	// 新規ファイルを一覧へ反映する
}

CharacterEditor::CharacterEditor()
{
}
