#pragma once

// ※ ImGui / DirectXTK(SimpleMath) は既存のPCH等で読み込まれている前提です。
//    ImGuizmo は本ファイルでのみ使うため明示的にインクルードします。
#include "ImGuizmo.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップに配置する1オブジェクト分のデータ(仮実装：Transformのみ)
// 実際のプロジェクトでは KdGameObject 等の実体への参照/IDに差し替える想定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct MapObject
{
	std::string					name = "Object";

	DirectX::SimpleMath::Vector3	pos = { 0,0,0 };
	DirectX::SimpleMath::Vector3	rotate = { 0,0,0 };	// 度数法(degree) X,Y,Z
	DirectX::SimpleMath::Vector3	scale = { 1,1,1 };

	std::string	modelPath;		// 読み込んだモデルのファイルパス(JSON保存/表示用)
	KdModelWork	modelWork;		// 実際に描画・当たり判定に使うモデルの実体

	// pos/rotate/scale から4x4行列を生成
	DirectX::SimpleMath::Matrix GetMatrix() const;

	// モデルファイルを読み込んで modelWork にセットする
	//	KdAssets::Instance().m_modeldatas (KdDataStorage) を直接経由することで、
	//	読込失敗時にクラッシュせずログを出して抜けられるようにしている
	void SetModel(const std::string& path)
	{
		modelPath = path;
		if (path.empty()) return;

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

	// 選択中オブジェクトを注視点とするプレビュー専用ウィンドウ(RenderPreviewViewport()が描いた絵を表示する)
	void DrawPreviewWindow();

	// 配置済みオブジェクトを描画する実処理(DrawPlacedObjects()・RenderPreviewViewport()の共通部分)
	void DrawObjects();

	void AddObject();
	void RemoveSelected();

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