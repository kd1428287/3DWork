#pragma once

#include "nlohmann/json.hpp"
#include "Application/Definitions/Map/MapData.h"
#include "Application/Definitions/Physics/ColliderParams.h"
#include "../ThirdParty/ImGuizmo.h"
#include "../Common/EditorPreviewViewport.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// コライダー調整エディタ
//	Prefab(JSONファイル)とマップ配置オブジェクトの"Collider"コンポーネントを、
//	モデルと重ねたワイヤー表示を見ながら編集する。
//	編集中はColliderParams(ColliderParams.h)を直接いじり、変更のたびにJSONへ書き戻す。
//	EditorHost::Draw() から Update()、EditorHost::RenderPreviewViewports() から
//	RenderPreviewViewport() を呼ぶ
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class ColliderEditor
{
public:

	void Update();
	void RenderPreviewViewport();

	// このエディタのウィンドウにフォーカスがあるか(Ctrl+Zなどのショートカットを他エディタと奪い合わないため)
	bool IsFocused() const { return m_focused; }

private:

	enum class Source { Prefab, Map };

	struct PrefabNode
	{
		std::string				label;
		nlohmann::ordered_json* node = nullptr;
		int						parent = -1;	// m_prefabNodes内の親index(ルートは-1)
	};

	// Prefab対象の位置合わせ用に表示する親オブジェクト(ルートまで)
	struct AncestorPreview
	{
		KdModelWork		model;
		Math::Matrix	world = Math::Matrix::Identity;
		ColliderParams	collider;
		bool			hasCollider = false;
	};

	// ---- ウィンドウ ----
	void DrawTargets();
	void DrawPrefabTab();
	void DrawMapTab();
	void DrawInspector();
	void DrawShapeInspector(ColliderShapeParams& s, bool& changed);
	void DrawPreviewWindow();

	// 選択中の形状を操作するギズモ(Previewウィンドウ内で呼ぶ)
	void DrawGizmo();

	// ---- 対象の読み込み・選択 ----
	bool LoadPrefab(const std::string& path);
	void SavePrefab();
	void CollectPrefabNodes(nlohmann::ordered_json& node, const std::string& prefix, int parent);
	void SelectPrefabNode(int index);
	void SelectMapObject(ObjectId id);
	void ClearTarget();
	void ReadParams(const nlohmann::json& paramsJson);

	// 現在の対象(JSON側)へm_paramsを書き戻す。Colliderが無ければ追加する
	void WriteBack();

	nlohmann::ordered_json* CurrentPrefabNode();
	void LoadPreviewModel(const std::string& path);
	KdModelWork LoadModelWork(const std::string& path);

	// Prefabノードのワールド行列。ルートは位置・回転を無視してスケールだけ反映し、子は親の行列を掛ける。
	// AttachToBoneを持つノードは、親モデルのボーンに追従した位置になる
	Math::Matrix ComputeNodeWorld(int index);

	// AttachToBoneノードのワールド行列を求める(実行時のAttachToSocketComponentと同じ計算)。ボーンが取れなければfalse
	bool TryComputeBoneAttachedWorld(int parentIndex, const Math::Matrix& parentWorld,
		const nlohmann::ordered_json& node, const nlohmann::ordered_json& attach, Math::Matrix& out);
	KdModelWork* GetPreviewModel();

	// ---- 編集操作 ----
	void PushUndo();
	void Undo();
	void Redo();
	void ClampSelection();
	std::string MakeUniqueName(const std::string& base) const;

	// ---- プレビュー ----
	Math::Vector3 ComputeFocus() const;
	void DrawShapeWires();

	static constexpr size_t kMaxUndoDepth = 50;

	Source	m_source = Source::Prefab;
	bool	m_focused = false;

	// Prefab
	char						m_prefabPathBuf[260] = "Asset/Data/Prefab/Player.json";	// ※実際の置き場所に合わせる
	nlohmann::ordered_json		m_prefabRoot;
	std::vector<PrefabNode>		m_prefabNodes;
	int							m_prefabIndex = -1;
	bool						m_prefabLoaded = false;
	bool						m_prefabDirty = false;

	// Map
	ObjectId	m_mapSelectedId = kInvalidObjectId;

	// 現在の編集対象
	bool			m_targetValid = false;
	bool			m_hasCollider = false;
	std::string		m_targetLabel;
	ColliderParams	m_params;
	int				m_selectedShape = -1;
	int				m_addType = 1;	// 追加時の形状(0:Sphere 1:Capsule 2:Box)

	std::vector<ColliderParams>	m_undoStack;
	std::vector<ColliderParams>	m_redoStack;

	// プレビュー
	EditorPreviewViewport	m_preview;
	KdModelWork				m_modelWork;	// Prefab対象のモデル(Map対象はMapObject側を使う)
	std::string				m_modelPath;
	Math::Matrix			m_targetWorld = Math::Matrix::Identity;	// 編集対象のワールド行列(形状はこの空間に置かれる)
	Math::Vector3			m_previewTarget = { 0, 0, 0 };
	bool					m_showModel = true;
	bool					m_showParent = true;			// 親のモデルを表示する
	bool					m_showParentColliders = true;	// 親のコライダーも(青で)表示する
	std::vector<AncestorPreview>	m_ancestors;

	// ギズモ操作
	bool				m_showGizmo = true;
	ImGuizmo::OPERATION	m_operation = ImGuizmo::TRANSLATE;	// TRANSLATE:offset等の移動 / SCALE:Boxはhalf extents、他は半径
	int					m_capsuleHandle = 0;				// Capsuleの移動対象(0:Start 1:End 2:両方)
	bool				m_gizmoWasUsing = false;
	bool				m_useSnap = false;
	float				m_snapValue = 0.05f;
	KdDebugWireFrame		m_wire;

	//=====================================================
	// シングルトンパターン
	//=====================================================
private:
	ColliderEditor();
	~ColliderEditor() {}

public:
	static ColliderEditor& Instance() {
		static ColliderEditor instance;
		return instance;
	}
};