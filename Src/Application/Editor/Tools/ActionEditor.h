#pragma once

#include "nlohmann/json.hpp"
#include "Application/Definitions/Loaders/EnemyAttackTableLoader.h"
#include "Application/Definitions/Physics/ColliderParams.h"
#include "Application/Components/Graphics/Animation/RootMotionExtractor.h"
#include "../Common/EditorPreviewViewport.h"
#include "Application/Components/Graphics/Render/WireFrameComponent.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 敵の攻撃テーブル(EnemyAttackTable)を、アニメーションと判定ワイヤーを見ながら調整するエディタ
//	Prefab(Warrock.json等)からモデル・武器スロット・攻撃テーブルのパスを読み、
//	攻撃テーブルJSONを編集・保存する。時間はゲームと同じ秒、区間はクリップのフレームで扱う。
//	EditorHost::Draw() から Update()、EditorHost::RenderPreviewViewports() から
//	RenderPreviewViewport() を呼ぶ
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class ActionEditor
{
public:

	void Update();
	void RenderPreviewViewport();

	// このエディタのウィンドウにフォーカスがあるか(Ctrl+Zなどのショートカットを他エディタと奪い合わないため)
	bool IsFocused() const { return m_focused; }

private:

	enum class Pool { Attacks, GapClosers };

	// Prefabの武器子オブジェクト1つ分(スロット名・ボーン追従・HitBox形状)
	struct SlotInfo
	{
		std::string		slot;
		std::string		bone;
		Math::Vector3	attachPos = { 0, 0, 0 };
		Math::Quaternion	attachRot = Math::Quaternion::Identity;
		Math::Vector3	selfScale = { 1, 1, 1 };
		std::vector<ColliderShapeParams>	hitShapes;
	};

	// ---- ウィンドウ ----
	void DrawTargets();
	void DrawPoolList(const char* title, Pool pool);
	void DrawInspector();
	void DrawSegments(MotionClipData& clip, float markerSec);
	void DrawTimeline();
	void DrawGameTimeStrip(EnemyAttackDefinition& atk);
	void DrawClipStrip(EnemyAttackDefinition& atk);
	void DrawPreviewWindow();

	// ---- データ ----
	bool LoadPrefab(const std::string& path);
	void Save();
	void CollectAnimNames();

	std::vector<EnemyAttackDefinition>& PoolVec(Pool pool);
	EnemyAttackDefinition* CurrentAttack();
	const SlotInfo* FindSlot(const std::string& slot) const;
	std::string MakeUniqueName(const std::string& base) const;
	void SelectAttack(Pool pool, int index);
	void ClampSelection();

	// ---- 編集操作 ----
	void PushUndo();
	void Undo();
	void Redo();
	void MarkEdited();

	// ---- プレビュー ----
	void EvaluatePose();
	void DrawHitboxWires();

	static constexpr size_t kMaxUndoDepth = 50;

	// 読み込み状態
	char			m_prefabPathBuf[260] = "Asset/Data/Prefab/Warrock.json";	// ※実際の置き場所に合わせる
	bool			m_loaded = false;
	bool			m_dirty = false;
	bool			m_focused = false;
	std::string		m_tablePath;
	std::string		m_tableComment;

	EnemyAttackTable			m_table;
	std::vector<std::string>	m_animNames;	// テーブル内で使われているクリップ名(選択候補)
	std::vector<std::string>	m_recoilChains;	// PrefabのBoneOffsetに定義された鎖名(跳ね返しの選択候補)
	std::vector<SlotInfo>		m_slots;

	// 選択
	Pool	m_pool = Pool::Attacks;
	int		m_index = -1;
	int		m_selectedPhase = 0;	// クリップストリップに表示するフェーズ

	std::vector<EnemyAttackTable>	m_undoStack;
	std::vector<EnemyAttackTable>	m_redoStack;

	// 再生
	float	m_time = 0.0f;
	bool	m_playing = false;
	bool	m_loop = true;
	bool	m_draggingTimeline = false;	// ドラッグ中は表示スケールを固定する
	float	m_viewSec = 2.0f;

	// プレビュー
	EditorPreviewViewport	m_preview;
	KdModelWork				m_model;
	std::string				m_modelPath;
	RootMotionConfig		m_rootMotionConfig;
	Math::Vector3			m_rootScale = { 1, 1, 1 };
	Math::Matrix			m_modelWorld = Math::Matrix::Identity;
	Math::Vector3			m_rootMotionAccum = { 0, 0, 0 };	// 現在時刻までのルートモーション移動量
	Math::Vector3			m_previewTarget = { 0, 1.5f, 0 };
	bool					m_poseDirty = true;
	int						m_phaseAtTime = 0;
	bool					m_showInactiveHitbox = true;
	KdDebugWireFrame		m_wire;

	//=====================================================
	// シングルトンパターン
	//=====================================================
private:
	ActionEditor();
	~ActionEditor() {}

public:
	static ActionEditor& Instance() {
		static ActionEditor instance;
		return instance;
	}
};