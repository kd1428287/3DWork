#pragma once

#include <string>
#include "Application/Definitions/Character/Common/CharacterData.h"
#include "../Common/UndoHistory.h"
#include "../Common/FileWatcher.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 開いているキャラクター(Game/*.json)の唯一のドキュメント。
//	Character/Collider/Actionの各エディタが同じインスタンスを見る(別々のコピーを持たない)。
//	・Undoは持ち主のここに積む。変更前にPushUndo()を呼ぶこと
//	・dirtyは保存済みJSONとの比較で決める(Undoで保存状態に戻れば外れる)
//	・内容が変わるたびにRevision()が増える。他エディタはこれで再取得の要否を判断する
//	・ファイルを開くのはCharacterEditorだけ。他エディタは開いているものを使う
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class CharacterDocument
{
public:
	static CharacterDocument& Instance();

	// --- データ ---
	CharacterFile& File() { return m_file; }
	const std::string& Path() const { return m_path; }
	CharacterNode* FindNode(NodeId id);
	CharacterNode* FindParent(NodeId id);

	// いま編集対象のノード(エディタ間で共有)
	NodeId SelectedId() const { return m_selectedId; }
	void SetSelected(NodeId id) { m_selectedId = id; }
	CharacterNode* Selected() { return FindNode(m_selectedId); }

	// --- ファイル ---
	bool Open(const std::string& path);
	void New();
	bool Save() { return SaveAs(m_path); }
	bool SaveAs(const std::string& path);

	// --- 変更管理 ---
	void PushUndo();	// データを書き換える直前に呼ぶ
	bool Undo();
	bool Redo();
	bool CanUndo() const { return m_history.CanUndo(); }
	bool CanRedo() const { return m_history.CanRedo(); }

	// 他エディタがFile()を直接書き換えた直後に呼ぶと、Revisionとdirtyに即反映される
	void Touch() { Sync(); }

	bool IsDirty();			// 一定間隔でキャッシュされた値(表示用)
	bool IsDirtyNow();		// その場で比較し直す(確認ダイアログ等の判断用)
	uint32_t Revision();

	// 毎フレーム呼んでよい(1フレーム1回だけ処理する)。外部ファイル更新の検知とホットリロード
	void Tick(float deltaTime);

	FileWatcher& Watcher() { return m_watcher; }

private:
	CharacterDocument();

	void Sync();					// 現在の内容を基準に、Revisionとdirtyを更新する
	void Refresh(bool force);		// 内容が変わっていればRevisionを進め、dirtyを再評価する

	struct Snapshot
	{
		CharacterFile	file;
		NodeId			selectedId = kInvalidNodeId;
	};

	CharacterFile			m_file;
	std::string				m_path;
	NodeId					m_selectedId = kInvalidNodeId;

	UndoHistory<Snapshot>	m_history;
	FileWatcher				m_watcher;

	nlohmann::json			m_current;		// 直近に確認した内容
	nlohmann::json			m_saved;		// 保存済みの内容
	bool					m_dirty = false;
	uint32_t				m_revision = 0;
	double					m_lastRefreshTime = -1.0;
	int						m_tickedFrame = -1;
};
