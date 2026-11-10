#include "Application/main.h"

#include "CharacterEditor.h"
#include "../Common/ComponentInspector.h"
#include "Application/Core/EventBus/Events/SceneEvents.h"

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
// ノード検索(再帰)
//=====================================================
static CharacterNode* FindNodeIn(CharacterNode& n, NodeId id)
{
	if (n.id == id) return &n;
	for (auto& c : n.children)
	{
		if (CharacterNode* r = FindNodeIn(c, id)) return r;
	}
	return nullptr;
}

static CharacterNode* FindParentIn(CharacterNode& n, NodeId id)
{
	for (auto& c : n.children)
	{
		if (c.id == id) return &n;
		if (CharacterNode* r = FindParentIn(c, id)) return r;
	}
	return nullptr;
}

CharacterNode* CharacterEditor::FindNode(NodeId id)
{
	if (id == kInvalidNodeId) return nullptr;
	return FindNodeIn(m_file.root, id);
}

CharacterNode* CharacterEditor::FindParent(NodeId id)
{
	if (id == kInvalidNodeId) return nullptr;
	return FindParentIn(m_file.root, id);
}

//=====================================================
// Undo / Redo
//=====================================================
void CharacterEditor::PushUndo()
{
	UndoState s;
	s.root = m_file.root;
	s.selectedId = m_selectedId;
	m_undoStack.push_back(std::move(s));
	if (m_undoStack.size() > kMaxUndoDepth) { m_undoStack.erase(m_undoStack.begin()); }

	m_redoStack.clear();
	m_dirty = true;
}

void CharacterEditor::Undo()
{
	if (m_undoStack.empty()) return;

	UndoState cur;
	cur.root = m_file.root;
	cur.selectedId = m_selectedId;
	m_redoStack.push_back(std::move(cur));

	UndoState prev = std::move(m_undoStack.back());
	m_undoStack.pop_back();
	m_file.root = std::move(prev.root);
	m_selectedId = prev.selectedId;
	m_dirty = true;
}

void CharacterEditor::Redo()
{
	if (m_redoStack.empty()) return;

	UndoState cur;
	cur.root = m_file.root;
	cur.selectedId = m_selectedId;
	m_undoStack.push_back(std::move(cur));

	UndoState next = std::move(m_redoStack.back());
	m_redoStack.pop_back();
	m_file.root = std::move(next.root);
	m_selectedId = next.selectedId;
	m_dirty = true;
}

//=====================================================
// 毎フレーム更新
//=====================================================
void CharacterEditor::Update()
{
	m_focusedNow = false;

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

	CheckHotReload();

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

	ImGui::InputText("Path", m_filePathBuf, sizeof(m_filePathBuf));

	if (ImGui::Button("Save (Ctrl+S)")) { Save(m_filePathBuf); }
	ImGui::SameLine();
	if (ImGui::Button("Load")) { RequestOpen(m_filePathBuf); }
	ImGui::SameLine();
	if (ImGui::Button("New")) { RequestNew(); }
	ImGui::SameLine();
	ImGui::Checkbox("Auto Reload", &m_autoReload);
	ImGui::SameLine();
	ImGui::Checkbox("Reload Scene On Save", &m_reloadSceneOnSave);

	ImGui::BeginDisabled(m_undoStack.empty());
	if (ImGui::Button("Undo (Ctrl+Z)")) { Undo(); }
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(m_redoStack.empty());
	if (ImGui::Button("Redo (Ctrl+Y)")) { Redo(); }
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (m_dirty) { ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "* 未保存の変更あり"); }

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
		const bool isCurrent = (e.path == m_filePathBuf);
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
	if (node.id == m_selectedId) { flags |= ImGuiTreeNodeFlags_Selected; }

	const std::string label = node.name + "##" + std::to_string(node.id);
	const bool open = ImGui::TreeNodeEx(label.c_str(), flags);

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) { m_selectedId = node.id; }

	// ルート以外はドラッグで親を付け替えられる
	if (node.id != m_file.root.id && ImGui::BeginDragDropSource())
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

	const bool isRoot = (m_selectedId == m_file.root.id);

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

	DrawNodeTree(m_file.root);

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

	// name/components/children以外のキー(保存時にそのまま書き戻される)
	if (node->extra.is_object() && !node->extra.empty())
	{
		if (ImGui::TreeNode("Extra Fields (read only)"))
		{
			ImGui::TextWrapped("%s", node->extra.dump(2).c_str());
			ImGui::TreePop();
		}
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
	if (!parent) { parent = &m_file.root; }

	PushUndo();

	CharacterNode child;
	child.id = m_file.nextId++;
	child.name = "Child" + std::to_string(child.id);
	const NodeId newId = child.id;
	parent->children.push_back(std::move(child));
	m_selectedId = newId;
}

void CharacterEditor::RemoveSelected()
{
	if (m_selectedId == m_file.root.id) return;

	CharacterNode* parent = FindParent(m_selectedId);
	if (!parent) return;

	PushUndo();

	const NodeId id = m_selectedId;
	parent->children.erase(
		std::remove_if(parent->children.begin(), parent->children.end(),
			[id](const CharacterNode& c) { return c.id == id; }),
		parent->children.end());

	m_selectedId = kInvalidNodeId;
}

void CharacterEditor::DuplicateSelected()
{
	if (m_selectedId == m_file.root.id) return;

	CharacterNode* src = FindSelected();
	CharacterNode* parent = FindParent(m_selectedId);
	if (!src || !parent) return;

	PushUndo();

	CharacterNode copy = *src;
	copy.name += "_copy";
	ReassignNodeIds(copy, m_file.nextId);
	const NodeId newId = copy.id;

	// insertで参照が無効になるので、位置を先に求めてから挿入する
	auto it = std::find_if(parent->children.begin(), parent->children.end(),
		[this](const CharacterNode& c) { return c.id == m_selectedId; });
	parent->children.insert(it + 1, std::move(copy));
	m_selectedId = newId;
}

void CharacterEditor::MoveSelected(int delta)
{
	CharacterNode* parent = FindParent(m_selectedId);
	if (!parent) return;

	auto& v = parent->children;
	auto it = std::find_if(v.begin(), v.end(),
		[this](const CharacterNode& c) { return c.id == m_selectedId; });
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
	if (src == m_file.root.id || src == newParent) return;

	CharacterNode* s = FindNode(src);
	CharacterNode* oldParent = FindParent(src);
	if (!s || !oldParent) return;
	if (FindNodeIn(*s, newParent)) return;	// 子孫への移動は循環になる

	PushUndo();

	CharacterNode moved = std::move(*s);
	oldParent->children.erase(
		std::remove_if(oldParent->children.begin(), oldParent->children.end(),
			[src](const CharacterNode& c) { return c.id == src; }),
		oldParent->children.end());

	// erase後は参照が無効なので、親を探し直す
	if (CharacterNode* np = FindNode(newParent))
	{
		np->children.push_back(std::move(moved));
	}
}

//=====================================================
// ファイル操作
//=====================================================
void CharacterEditor::RequestOpen(const std::string& path)
{
	if (!m_dirty) { OpenFile(path); return; }

	m_pendingPath = path;
	m_pendingIsNew = false;
	m_openPopupRequested = true;
}

void CharacterEditor::RequestNew()
{
	if (!m_dirty) { NewFile(); return; }

	m_pendingIsNew = true;
	m_openPopupRequested = true;
}

void CharacterEditor::OpenFile(const std::string& path)
{
	CharacterFile f;
	if (!LoadCharacterFile(path, f))
	{
		KdDebugGUI::Instance().AddLog("CharacterEditor: 読み込み失敗 %s\n", path.c_str());
		return;
	}

	// IDは出現順採番なので、構造が同じ再読込なら選択中IDもそのまま有効
	const NodeId keep = m_selectedId;

	m_file = std::move(f);
	strncpy_s(m_filePathBuf, path.c_str(), _TRUNCATE);

	m_selectedId = FindNode(keep) ? keep : m_file.root.id;
	m_undoStack.clear();
	m_redoStack.clear();
	m_dirty = false;

	FILETIME writeTime;
	if (JsonLoader::GetLastWriteTime(path, writeTime)) { m_lastWriteTime = writeTime; }

	KdDebugGUI::Instance().AddLog("CharacterEditor: 読み込みました %s\n", path.c_str());
}

void CharacterEditor::NewFile()
{
	m_file = CharacterFile();
	m_file.root.id = m_file.nextId++;
	m_file.root.name = "NewCharacter";

	strncpy_s(m_filePathBuf, "Asset/Data/Game/NewCharacter.json", _TRUNCATE);

	m_selectedId = m_file.root.id;
	m_undoStack.clear();
	m_redoStack.clear();
	m_dirty = false;
	m_lastWriteTime = {};
}

void CharacterEditor::Save(const std::string& path)
{
	if (!SaveCharacterFile(path, m_file))
	{
		KdDebugGUI::Instance().AddLog("CharacterEditor: 保存に失敗 %s\n", path.c_str());
		return;
	}

	// 自分の保存を外部変更と誤検知しない
	FILETIME writeTime;
	if (JsonLoader::GetLastWriteTime(path, writeTime)) { m_lastWriteTime = writeTime; }

	m_dirty = false;
	m_listLoaded = false;	// 新規ファイルを一覧へ反映する

	KdDebugGUI::Instance().AddLog("CharacterEditor: 保存しました %s\n", path.c_str());

	if (m_reloadSceneOnSave) { GLOBALEVENT->Publish(Events::ReloadingScene()); }
}

void CharacterEditor::CheckHotReload()
{
	if (!m_autoReload) return;

	m_reloadCheckTimer += Application::Instance().GetDeltaTime();
	if (m_reloadCheckTimer < 0.5f) return;
	m_reloadCheckTimer = 0.0f;

	FILETIME writeTime;
	if (!JsonLoader::GetLastWriteTime(m_filePathBuf, writeTime)) return;

	if (m_lastWriteTime.dwLowDateTime == 0 && m_lastWriteTime.dwHighDateTime == 0)
	{
		m_lastWriteTime = writeTime;
		return;
	}
	if (CompareFileTime(&writeTime, &m_lastWriteTime) == 0) return;

	m_lastWriteTime = writeTime;

	// 編集中の内容を黙って上書きしない
	if (m_dirty)
	{
		KdDebugGUI::Instance().AddLog("CharacterEditor: 外部変更を検知しましたが未保存の編集があるため再読込しません\n");
		return;
	}

	OpenFile(m_filePathBuf);
}

//=====================================================
// コンストラクタ:起動時に既定のキャラクターを読み込む
//=====================================================
CharacterEditor::CharacterEditor()
{
	// 読込失敗時でも空のルートが残るよう、先にNewFile()してから既定パスを開く
	const std::string defaultPath = m_filePathBuf;
	NewFile();
	OpenFile(defaultPath);
}
