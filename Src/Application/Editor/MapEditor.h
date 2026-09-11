#pragma once

// ※ ImGui / DirectXTK(SimpleMath) は既存のPCH等で読み込まれている前提です。
//    ImGuizmo は本ファイルでのみ使うため明示的にインクルードします。
#include "ImGuizmo.h"
#include "../Factories/Map/ComponentTypes.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップに配置する1オブジェクト分のデータ
//	Transform + 任意個数のコンポーネント構成を持つ。実際のGameObjectへの実体化は
//	TerrainFactory側がComponentRegistry経由で行う(このファイルは実コンポーネントの詳細を知らない)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct MapObject
{
	std::string					name = "Object";

	DirectX::SimpleMath::Vector3	pos = { 0,0,0 };
	DirectX::SimpleMath::Vector3	rotate = { 0,0,0 };	// 度数法(degree) X,Y,Z
	DirectX::SimpleMath::Vector3	scale = { 1,1,1 };

	std::vector<ComponentEntry>	components;	// JSON保存されるコンポーネント構成

	KdModelWork	modelWork;		// プレビュー表示専用。componentsの"ModelRender"から同期する

	// pos/rotate/scale から4x4行列を生成
	DirectX::SimpleMath::Matrix GetMatrix() const;

	// 指定した種類のコンポーネントを持っているか
	bool HasComponent(const std::string& type) const
	{
		for (auto& c : components) { if (c.type == type) return true; }
		return false;
	}

	// components内の"ModelRender"コンポーネントのmodelパラメータを見て、
	// プレビュー用のmodelWorkを読み込み直す。
	// コンポーネントの追加/削除/パラメータ編集のたびに呼び出すこと
	void SyncPreviewModel()
	{
		std::string path;
		for (auto& c : components)
		{
			if (c.type == "ModelRender")
			{
				path = c.params.value("model", std::string());
				break;
			}
		}

		if (path.empty())
		{
			modelWork = KdModelWork();	// 未割り当てに戻す
			return;
		}

		std::shared_ptr<KdModelData> data = KdAssets::Instance().m_modeldatas.GetData(path);
		if (!data)
		{
			KdDebugGUI::Instance().AddLog("MapEditor: モデル読み込み失敗 %s\n", path.c_str());
			return;
		}

		modelWork.SetModelData(data);
	}
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップエディタ本体
//	KdDebugGUI::GuiProcess() の中(ImGui::NewFrame() 後 ～ ImGui::Render() 前)から
//	Update() を呼び出して使用する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class MapEditor
{
public:

	// 毎フレームの更新・描画(ImGuiウィンドウ + ギズモ)
	void Update();

	// 配置済みオブジェクト一覧の取得(読み取り専用参照用)
	const std::vector<MapObject>& GetObjects() const { return m_objects; }

	// 配置済みオブジェクトを実際に3D描画する
	//	SceneManager::Draw() など、GUIとは別の3D描画パスから呼び出すこと
	//	(KdDebugGUI::GuiProcess() 内のUpdate()から呼んではいけない)
	void DrawPlacedObjects();

	// 編集中マップ全体を、専用カメラ・専用のオフスクリーンバッファへ描画する
	//	EffectEditorのRenderPreviewViewport()と同じパターン。
	//	3D描画パス側(メインシーンの描画・DrawPlacedObjects()とは別のタイミング)から
	//	フレームに1回呼ぶこと。呼び出し前後でRT/ビューポート/カメラ定数バッファを
	//	退避・復元するため、メインシーン(EditorViewport)の描画状態には一切影響しない
	void RenderPreviewViewport();

private:

	void DrawMainMenu();
	void DrawHierarchy();
	void DrawInspector();
	void DrawGizmo();
	void DrawAssetPicker();

	// Inspector内、選択中オブジェクトのコンポーネント一覧(追加/削除/パラメータ編集)
	void DrawComponentList(MapObject& obj);

	// ComponentTypeInfo::schemaに従ってparamsのフィールドを自動描画する(v1の汎用UI)。
	// 戻り値は「このフレームで何か編集されたか」
	bool DrawComponentParamsGeneric(nlohmann::json& params, const ComponentTypeInfo& info);

	// 選択中オブジェクトを注視点とするプレビュー専用ウィンドウ(RenderPreviewViewport()が描いた絵を表示する)
	void DrawPreviewWindow();

	// 配置済みオブジェクトを描画する実処理(DrawPlacedObjects()・RenderPreviewViewport()の共通部分)
	void DrawObjects();

	// プレビュー用カメラの注視点(選択中オブジェクトがあればその位置、無ければ配置済み全体の重心)
	//	RenderPreviewViewport()・DrawGizmo()の両方で同じ注視点を使うための共通処理
	DirectX::SimpleMath::Vector3 GetPreviewTarget() const;

	void AddObject();
	void RemoveSelected();

	//=====================================================
	// Undo / Redo
	//	スナップショット方式(m_objects全体のコピーを積む)。
	//	オブジェクト数が数百程度までの想定なら十分軽量なので、
	//	差分ベースのコマンドパターンにはせずシンプルに実装している
	//=====================================================
	struct UndoState
	{
		std::vector<MapObject>	objects;
		int						selected = -1;
	};

	static constexpr size_t kMaxUndoDepth = 50;

	std::vector<UndoState>	m_undoStack;
	std::vector<UndoState>	m_redoStack;

	// ドラッグ系操作(ギズモ・Inspectorのスライダー)が「今まさに操作中」かどうかの前フレーム値。
	// 操作開始の一瞬だけPushUndo()するために使う(毎フレーム積むと1ドラッグで大量の履歴になる為)
	bool	m_gizmoWasUsing = false;

	// 現在の状態をUndoスタックへ退避する(Redoスタックはクリアされる)。
	// 「変更を加える直前」に呼ぶこと
	void PushUndo();
	void Undo();
	void Redo();

	void Save(const std::string& path);
	void Load(const std::string& path);

	// マップデータ(JSON)の外部変更を検知して自動リロードする
	void CheckHotReload();

	// 登録済みアセット一覧の読み込み/保存(JSON)
	void LoadModelRegistry(const std::string& path);
	void SaveModelRegistry(const std::string& path);

	// Windowsのファイル選択ダイアログでモデルファイルを1つ登録する
	void AddModelViaFileDialog();

	// 登録済みアセット一覧から指定indexを除外する(ファイル自体は削除しない)
	void RemoveRegisteredModel(int index);

	std::vector<MapObject>	m_objects;
	int						m_selected = -1;

	ImGuizmo::OPERATION			m_operation = ImGuizmo::TRANSLATE;
	ImGuizmo::MODE				m_mode = ImGuizmo::WORLD;

	bool	m_useSnap = false;
	float	m_snapValue[3] = { 1.0f, 1.0f, 1.0f };

	char	m_filePathBuf[260] = "Asset/Data/Map/MapData.json";

	// アセット一覧(登録済みモデルファイルパス)
	std::vector<std::string>	m_modelFileList;
	bool						m_assetListLoaded = false;
	int							m_selectedAsset = -1;						// 一覧内での選択(削除用)
	char						m_registryPathBuf[260] = "Asset/Data/Map/ModelAssets.json";	// 登録一覧の保存先
	static constexpr const char* kAssetsFilePath = "Asset/Models/";

	// ホットリロード関連
	bool		m_autoReload = true;	// trueなら外部変更を自動検知
	float		m_reloadCheckTimer = 0.0f;	// ポーリング間隔調整用
	FILETIME	m_lastWriteTime = {};		// 最後に確認したファイル更新日時

	//=====================================================
	// Map Preview 専用ビューポート
	//	EditorViewport(ゲーム画面をオフスクリーン→ImGui::Imageで表示するクラス)や
	//	EffectEditor::PreviewViewportと全く同じパターンを踏襲した、マップ全体プレビュー用の
	//	ミニビューポート。ゲームのメインシーン・ゲームカメラとは完全に独立している。
	//=====================================================
	struct PreviewViewport
	{
		std::shared_ptr<KdTexture>	Color;	// オフスクリーンのカラーバッファ
		std::shared_ptr<KdTexture>	Depth;	// オフスクリーンのZバッファ

		int		Width = 0;
		int		Height = 0;

		ImVec2	ScreenPos = { 0,0 };	// ウィンドウ内、画像の左上スクリーン座標
		ImVec2	ScreenSize = { 0,0 };	// ウィンドウ内、画像の表示サイズ

		// ウィンドウの表示サイズに合わせてオフスクリーンバッファを作り直す(サイズ据え置きなら何もしない)
		void Resize(int w, int h);
	};

	// プレビュー専用の簡易オービットカメラ。
	// 選択中オブジェクトがあればその pos を、無ければ配置済みオブジェクト全体の重心
	// (オブジェクトが無ければ原点)を注視点とする
	struct PreviewCamera
	{
		float Distance = 5.0f;
		float Yaw = 0.0f;
		float Pitch = 0.3f;

		DirectX::SimpleMath::Matrix GetView(const DirectX::SimpleMath::Vector3& target) const;
		DirectX::SimpleMath::Matrix GetProj(float aspect) const;
	};

	PreviewViewport	m_previewViewport;
	PreviewCamera	m_previewCamera;

	// プレビューカメラの注視点キャッシュ。ギズモ操作中(ImGuizmo::IsUsing()中)は
	// 選択オブジェクトの座標そのものを注視点にせず、操作開始時点の値のまま固定する。
	// (毎フレーム選択オブジェクトの座標を注視点にすると、Translate操作でオブジェクトを
	//  動かした分だけカメラも一緒に追従してしまい、画面上では全く動いて見えなくなる為)
	mutable DirectX::SimpleMath::Vector3	m_previewTargetCache = { 0,0,0 };

	//=====================================================
	// シングルトンパターン
	//=====================================================
private:
	MapEditor();
	~MapEditor() {}

public:
	static MapEditor& Instance() {
		static MapEditor instance;
		return instance;
	}
};