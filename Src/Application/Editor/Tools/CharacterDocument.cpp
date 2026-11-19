#include "Application/main.h"

#include "CharacterDocument.h"
#include "../EditorHost.h"

namespace
{
	constexpr const char* kDefaultPath = "Asset/Data/Game/Player.json";
	constexpr double kRefreshInterval = 0.25;	// 秒
}

CharacterDocument& CharacterDocument::Instance()
{
	static CharacterDocument instance;
	return instance;
}

CharacterDocument::CharacterDocument()
{
	// 読込失敗時でも空のルートが残るよう、先にNew()してから既定パスを開く
	New();
	Open(kDefaultPath);
}

CharacterNode* CharacterDocument::FindNode(NodeId id)
{
	if (id == kInvalidNodeId) return nullptr;
	return FindNodeById(m_file.root, id);
}

CharacterNode* CharacterDocument::FindParent(NodeId id)
{
	if (id == kInvalidNodeId) return nullptr;
	return FindParentOf(m_file.root, id);
}

bool CharacterDocument::Open(const std::string& path)
{
	CharacterFile f;
	if (!LoadCharacterFile(path, f))
	{
		KdDebugGUI::Instance().AddLog("CharacterDocument: 読み込み失敗 %s\n", path.c_str());
		return false;
	}

	// IDは出現順採番なので、構造が同じ再読込なら選択中IDもそのまま有効
	const NodeId keep = m_selectedId;

	m_file = std::move(f);
	m_path = path;
	m_selectedId = FindNode(keep) ? keep : m_file.root.id;
	m_history.Clear();

	m_saved = CharacterFileToJson(m_file);
	Sync();
	m_watcher.Sync(m_path);

	KdDebugGUI::Instance().AddLog("CharacterDocument: 読み込みました %s\n", path.c_str());
	return true;
}

void CharacterDocument::New()
{
	m_file = CharacterFile();
	m_file.root.id = m_file.nextId++;
	m_file.root.name = "NewCharacter";
	m_path = "Asset/Data/Game/NewCharacter.json";
	m_selectedId = m_file.root.id;
	m_history.Clear();

	m_saved = CharacterFileToJson(m_file);
	Sync();
	m_watcher.Reset();
}

bool CharacterDocument::SaveAs(const std::string& path)
{
	if (!SaveCharacterFile(path, m_file))
	{
		KdDebugGUI::Instance().AddLog("CharacterDocument: 保存に失敗 %s\n", path.c_str());
		return false;
	}

	m_path = path;
	m_saved = CharacterFileToJson(m_file);
	Sync();

	// 自分の保存を外部変更と誤検知しない
	m_watcher.Sync(m_path);

	KdDebugGUI::Instance().AddLog("CharacterDocument: 保存しました %s\n", path.c_str());

	// シーンの再読み込みはエディタを閉じた時に行う
	EditorHost::Instance().MarkSceneDirty();
	return true;
}

void CharacterDocument::PushUndo()
{
	m_history.Push({ m_file, m_selectedId });
}

bool CharacterDocument::Undo()
{
	Snapshot cur{ m_file, m_selectedId };
	if (!m_history.Undo(cur)) return false;

	m_file = std::move(cur.file);
	m_selectedId = cur.selectedId;
	Sync();
	return true;
}

bool CharacterDocument::Redo()
{
	Snapshot cur{ m_file, m_selectedId };
	if (!m_history.Redo(cur)) return false;

	m_file = std::move(cur.file);
	m_selectedId = cur.selectedId;
	Sync();
	return true;
}

void CharacterDocument::Sync()
{
	m_current = CharacterFileToJson(m_file);
	++m_revision;
	m_dirty = (m_current != m_saved);
	m_lastRefreshTime = ImGui::GetTime();
}

void CharacterDocument::Refresh(bool force)
{
	const double now = ImGui::GetTime();
	if (!force && m_lastRefreshTime >= 0.0 && now - m_lastRefreshTime < kRefreshInterval) return;
	m_lastRefreshTime = now;

	nlohmann::json cur = CharacterFileToJson(m_file);
	if (cur != m_current)
	{
		m_current = std::move(cur);
		++m_revision;
	}
	m_dirty = (m_current != m_saved);
}

bool CharacterDocument::IsDirty()
{
	Refresh(false);
	return m_dirty;
}

bool CharacterDocument::IsDirtyNow()
{
	Refresh(true);
	return m_dirty;
}

uint32_t CharacterDocument::Revision()
{
	Refresh(false);
	return m_revision;
}

void CharacterDocument::Tick(float deltaTime)
{
	const int frame = ImGui::GetFrameCount();
	if (m_tickedFrame == frame) return;
	m_tickedFrame = frame;

	Refresh(false);

	if (!m_watcher.Poll(m_path, deltaTime)) return;

	// 編集中の内容を黙って上書きしない
	if (IsDirtyNow())
	{
		KdDebugGUI::Instance().AddLog("CharacterDocument: 外部変更を検知しましたが未保存の編集があるため再読込しません\n");
		return;
	}

	Open(m_path);
}
