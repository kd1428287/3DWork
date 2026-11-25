#pragma once

#include "CharacterDocument.h"
#include "../Common/EditorFileUI.h"

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
	bool Save();
	std::string MakeSavePath() const;

	// ノード操作(Idで探す)。データは共有のCharacterDocumentが持つ
	CharacterNode* FindSelected() { return m_doc.Selected(); }
	void AddChild();
	void RemoveSelected();
	void DuplicateSelected();
	void MoveSelected(int delta);
	void MoveNode(NodeId src, NodeId newParent);

	// Undo/Redoは共有ドキュメントが持つ
	void PushUndo() { m_doc.PushUndo(); }	// 変更を加える直前に呼ぶ
	void Undo() { m_doc.Undo(); }
	void Redo() { m_doc.Redo(); }

	CharacterDocument&	m_doc = CharacterDocument::Instance();

	// ファイル名欄の編集用バッファ。ドキュメント側のパスが変わった時だけ上書きする
	char		m_nameBuf[128] = "";
	std::string	m_lastDocPath;

	// キャラクター一覧(フォルダ走査)
	JsonFileList	m_list{ "Asset/Data/Game/Character/",
		[](const nlohmann::json& j) { return j.is_object() && j.contains("components") && j["components"].is_array(); } };

	// 未保存変更の確認ダイアログ用
	std::string	m_pendingPath;
	bool		m_pendingIsNew = false;
	bool		m_openPopupRequested = false;

	// ツリーのドラッグ&ドロップ(描画後にまとめて反映する)
	NodeId	m_pendingMoveSrc = kInvalidNodeId;
	NodeId	m_pendingMoveDst = kInvalidNodeId;

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
