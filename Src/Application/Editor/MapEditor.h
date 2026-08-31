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

private:

	void DrawMainMenu();
	void DrawHierarchy();
	void DrawInspector();
	void DrawGizmo();
	void DrawAssetPicker();

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

	char	m_filePathBuf[260] = "Asset/Data/map.json";

	// アセット一覧(登録済みモデルファイルパス)
	std::vector<std::string>	m_modelFileList;
	bool						m_assetListLoaded = false;
	int							m_selectedAsset = -1;						// 一覧内での選択(削除用)
	char						m_registryPathBuf[260] = "Asset/Data/ModelAssets.json";	// 登録一覧の保存先

	// ホットリロード関連
	bool		m_autoReload = true;	// trueなら外部変更を自動検知
	float		m_reloadCheckTimer = 0.0f;	// ポーリング間隔調整用
	FILETIME	m_lastWriteTime = {};		// 最後に確認したファイル更新日時

	//=====================================================
	// シングルトンパターン
	//=====================================================
private:
	MapEditor() {}
	~MapEditor() {}

public:
	static MapEditor& Instance() {
		static MapEditor instance;
		return instance;
	}
};