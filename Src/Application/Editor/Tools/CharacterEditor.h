#pragma once

#include "Application/Definitions/Character/Common/CharacterData.h"

// キャラクター(Prefab JSON)エディタ。MapEditorと同じくKdDebugGUI::GuiProcess()内からUpdate()を呼ぶ
class CharacterEditor
{
public:

	void Update();

	// いずれかのCharacterEditorウィンドウにフォーカスがあるか(他エディタのUndo/Redo抑止用)
	bool IsFocused() const { return m_focused; }

private:

	void DrawMainMenu();
	void DrawCharacterList();
	void DrawHierarchy();
	void DrawNodeTree(CharacterNode& node);
	void DrawInspector();

	// ファイル操作。未保存変更があれば確認ダイアログを挟む
	void RequestOpen(const std::string& path);
	void RequestNew();
	void OpenFile(const std::string& path);
	void NewFile();
	void Save(const std::string& path);
	void CheckHotReload();
	void RefreshList();

	// ノード操作(Idで探す)
	CharacterNode* FindNode(NodeId id);
	CharacterNode* FindParent(NodeId id);
	CharacterNode* FindSelected() { return FindNode(m_selectedId); }
	void AddChild();
	void RemoveSelected();
	void DuplicateSelected();
	void MoveSelected(int delta);
	void MoveNode(NodeId src, NodeId newParent);

	// Undo/Redo(ツリー全体のスナップショット方式)
	struct UndoState
	{
		CharacterNode	root;
		NodeId			selectedId = kInvalidNodeId;
	};
	static constexpr size_t kMaxUndoDepth = 50;
	std::vector<UndoState>	m_undoStack;
	std::vector<UndoState>	m_redoStack;
	void PushUndo();	// 変更を加える直前に呼ぶ
	void Undo();
	void Redo();

	CharacterFile	m_file;
	NodeId			m_selectedId = kInvalidNodeId;
	bool			m_dirty = false;

	char	m_filePathBuf[260] = "Asset/Data/Game/Player.json";

	// キャラクター一覧(フォルダ走査)
	struct ListEntry { std::string name; std::string path; };
	std::vector<ListEntry>	m_list;
	bool					m_listLoaded = false;
	char					m_listDirBuf[260] = "Asset/Data/Game/";

	// 未保存変更の確認ダイアログ用
	std::string	m_pendingPath;
	bool		m_pendingIsNew = false;
	bool		m_openPopupRequested = false;

	// ツリーのドラッグ&ドロップ(描画後にまとめて反映する)
	NodeId	m_pendingMoveSrc = kInvalidNodeId;
	NodeId	m_pendingMoveDst = kInvalidNodeId;

	// ホットリロード
	bool		m_autoReload = true;
	float		m_reloadCheckTimer = 0.0f;
	FILETIME	m_lastWriteTime = {};

	bool	m_reloadSceneOnSave = true;

	bool	m_focused = false;		// 前フレーム確定値
	bool	m_focusedNow = false;	// 今フレーム集計中

private:
	CharacterEditor();
	~CharacterEditor() {}

public:
	static CharacterEditor& Instance() {
		static CharacterEditor instance;
		return instance;
	}
};
