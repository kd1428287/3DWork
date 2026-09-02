#pragma once

// ※ ImGui / DirectXTK(SimpleMath) / KdEffekseerManager・KdEffekseerObject は
//    既存のPCH等で読み込まれている前提です。読み込まれていない場合は
//    #include "KdEffekseerManager.h" 等、実際の配置に合わせて追加してください。
#include "ImGuizmo.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップに配置する1エフェクト分のデータ(仮実装)
//	実体のパーティクル計算・描画はEffekseer(KdEffekseerManager)に委譲し、
//	ここでは「どこに・どの.efkを・どんな向き/速度/ループで置くか」のみを保持する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct EffectObject
{
	std::string	name = "Effect";
	std::string	effectPath;		// KdEffekseerManagerへ渡すファイル名(EffekseerPathからの相対パス)

	DirectX::SimpleMath::Vector3	pos = { 0,0,0 };
	DirectX::SimpleMath::Vector3	rotate = { 0,0,0 };	// 度数法(degree) X,Y,Z
	DirectX::SimpleMath::Vector3	scale = { 1,1,1 };

	float	speed = 1.0f;
	bool	loop = false;

	// エディタ上でプレビュー再生中のインスタンス(未再生ならlock()がnullptr)
	std::weak_ptr<KdEffekseerObject> wpPlaying;

	// 現在一時停止中かどうか(再生中のみ意味を持つ、JSONには保存しない)
	bool paused = false;

	// pos/rotate/scale から4x4行列を生成
	DirectX::SimpleMath::Matrix GetMatrix() const;

	// 現在プレビュー再生中かどうか
	bool IsPlaying() const;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エフェクト配置エディタ本体
//	KdDebugGUI::GuiProcess() の中(MapEditor::Update()と同じ場所)から
//	Update() を呼び出して使用する
//	※ ImGuizmo::BeginFrame() はMapEditor::Update()側で呼ばれている前提のため、
//	  ここでは呼ばない(1フレームに複数回呼ぶと内部状態がおかしくなるため)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class EffectEditor
{
public:

	// 毎フレームの更新・描画(ImGuiウィンドウ + ギズモ)
	void Update();

	// 配置済みエフェクト一覧の取得
	const std::vector<EffectObject>& GetObjects() const { return m_objects; }

	// ループ設定の配置エフェクトを一括再生/全プレビュー停止する
	//	ゲーム実行開始時・終了時(EditorViewportの表示切替タイミング等)から呼ぶ想定
	void PlayAllLooping();
	void StopAllPreview();

	// 配置済みの全エフェクトをプレビュー再生する(loop設定に関わらず全て)
	//	「Effect Editor」ウィンドウの Play All ボタンから呼ばれる
	void PlayAllPreview();

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

	// EffekseerPath 以下を走査して .efk ファイル一覧を更新する
	void RefreshEffectFileList();

	// プレビュー再生の開始/停止/一時停止
	void PlayPreview(EffectObject& obj);
	void StopPreview(EffectObject& obj);
	void SetPreviewPause(EffectObject& obj, bool pause);

	std::vector<EffectObject>	m_objects;
	int							m_selected = -1;

	ImGuizmo::OPERATION			m_operation = ImGuizmo::TRANSLATE;
	ImGuizmo::MODE				m_mode = ImGuizmo::WORLD;

	bool	m_useSnap = false;
	float	m_snapValue[3] = { 1.0f, 1.0f, 1.0f };

	char	m_filePathBuf[260] = "Asset/Data/effectmap.json";

	// アセット一覧(.efkファイルパス)
	std::vector<std::string>	m_effectFileList;
	bool						m_assetListLoaded = false;
	bool						m_autoPreviewOnSelect = true;	// Assetsで選択した瞬間にプレビュー再生するか

	// ホットリロード関連
	bool		m_autoReload = true;
	float		m_reloadCheckTimer = 0.0f;
	FILETIME	m_lastWriteTime = {};

	//=====================================================
	// シングルトンパターン
	//=====================================================
private:
	EffectEditor() {}
	~EffectEditor() {}

public:
	static EffectEditor& Instance() {
		static EffectEditor instance;
		return instance;
	}
};