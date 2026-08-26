#pragma once

// ※ ImGui / DirectXTK(SimpleMath) は既存のPCH等で読み込まれている前提です。
//    ImGuizmo は本ファイルでのみ使うため明示的にインクルードします。
#include "ImGuizmo.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップに配置する1オブジェクト分のデータ(仮実装：Transformのみ)
// 実際のプロジェクトでは GameObject 等の実体への参照/IDに差し替える想定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct MapObject
{
	std::string					name = "Object";

	DirectX::SimpleMath::Vector3	pos = { 0,0,0 };
	DirectX::SimpleMath::Vector3	rotate = { 0,0,0 };	// 度数法(degree) X,Y,Z
	DirectX::SimpleMath::Vector3	scale = { 1,1,1 };

	// pos/rotate/scale から4x4行列を生成
	DirectX::SimpleMath::Matrix GetMatrix() const;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップエディタ本体
//	DebugGUI::GuiProcess() の中(ImGui::NewFrame() 後 ～ ImGui::Render() 前)から
//	Update() を呼び出して使用する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class MapEditor
{
public:

	// 毎フレームの更新・描画(ImGuiウィンドウ + ギズモ)
	void Update();

	// 配置済みオブジェクト一覧の取得(実際の描画パスから参照する用)
	const std::vector<MapObject>& GetObjects() const { return m_objects; }

private:

	void DrawMainMenu();
	void DrawHierarchy();
	void DrawInspector();
	void DrawGizmo();

	void AddObject();
	void RemoveSelected();

	void Save(const std::string& path);
	void Load(const std::string& path);

	// マップデータ(JSON)の外部変更を検知して自動リロードする
	void CheckHotReload();

	std::vector<MapObject>	m_objects;
	int							m_selected = -1;

	ImGuizmo::OPERATION			m_operation = ImGuizmo::TRANSLATE;
	ImGuizmo::MODE				m_mode = ImGuizmo::WORLD;

	bool	m_useSnap = false;
	float	m_snapValue[3] = { 1.0f, 1.0f, 1.0f };

	char	m_filePathBuf[260] = "Asset/Data/Map/map.json";

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