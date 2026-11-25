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
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) { Save(); }
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
		strncpy_s(m_nameBuf, std::filesystem::path(m_lastDocPath).stem().string().c_str(), _TRUNCATE);
	}
	ImGui::InputText("File Name", m_nameBuf, sizeof(m_nameBuf));
	const std::string savePath = MakeSavePath();
	ImGui::TextDisabled("保存先: %s", savePath.empty() ? "(ファイル名を入力してください)" : savePath.c_str());

	ImGui::BeginDisabled(savePath.empty());
	if (ImGui::Button("Save (Ctrl+S)")) { Save(); }
	ImGui::EndDisabled();
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

	// 未保存変更の確認(保存に失敗したら続行しない)
	const auto choice = EditorFileUI::DrawUnsavedPopup(m_openPopupRequested, !savePath.empty());
	if (choice == EditorFileUI::UnsavedChoice::Discard || (choice == EditorFileUI::UnsavedChoice::Save && Save()))
	{
		if (m_pendingIsNew) { NewFile(); } else { OpenFile(m_pendingPath); }
	}

	ImGui::End();
}

//=====================================================
// キャラクター一覧(フォルダ以下の "components" 配列を持つJSON)
//=====================================================
void CharacterEditor::DrawCharacterList()
{
	ImGui::Begin("Characters");
	m_focusedNow |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	std::string path;
	if (m_list.Draw(m_doc.Path(), path)) { RequestOpen(path); }

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

// 開いているファイルと同じフォルダ(新規なら一覧のフォルダ)に "ファイル名.json" で保存する
std::string CharacterEditor::MakeSavePath() const
{
	namespace fs = std::filesystem;

	if (m_nameBuf[0] == '\0') return {};

	const fs::path dir = m_doc.Path().empty() ? fs::path(m_list.Dir()) : fs::path(m_doc.Path()).parent_path();
	return (dir / (std::string(m_nameBuf) + ".json")).generic_string();
}

bool CharacterEditor::Save()
{
	const std::string path = MakeSavePath();
	if (path.empty() || !m_doc.SaveAs(path)) return false;

	m_list.Invalidate();	// 新規ファイルを一覧へ反映する
	return true;
}

CharacterEditor::CharacterEditor()
{
}
