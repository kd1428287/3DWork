#pragma once

#include "ImGuizmo.h"
#include "../Effect/EffectParams.h"
#include "../Effect/EffectInstance.h"
#include "../Effect/KdAssetsTextureProvider.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップに配置する1エフェクト分のデータ
//	「どこに・どんなGPUパーティクルを・どう発生させるか」を保持する。
//	実体のパーティクル(EffectInstance)はプレビュー再生時に遅延生成し、
//	JSONにはparamsとtransformのみを保存する(プレビュー用の状態は保存しない)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct EffectObject
{
	std::string	name = "Effect";

	DirectX::SimpleMath::Vector3	pos = { 0,0,0 };
	DirectX::SimpleMath::Vector3	rotate = { 0,0,0 };	// 度数法(degree) X,Y,Z ※現状パーティクルの発生方向には未反映(下記TODO参照)
	DirectX::SimpleMath::Vector3	scale = { 1,1,1 };		// 現状は表示上のギズモ操作用。パーティクルサイズには未反映

	GPUParticleParams	params;

	//--------------------------------------------------
	// プレビュー再生用の実行時状態(JSONには保存しない)
	//	パーティクル本体・テクスチャ・Gravity適用はEffectInstanceが一元管理する。
	//	EffectDispatcher側の実行時インスタンスと全く同じクラスを使う為、
	//	プレビューと実際のゲーム内挙動が食い違うことは無い。
	//--------------------------------------------------
	EffectInstance	previewInstance;

	// UI上の再生状態(Play〜Stopの間はtrue)。
	// Burstリピート/Continuousの発生タイミング管理自体はEffectInstance::Play()/Update()が持つ為、
	// ここではEditor側の表示・入力ゲーティング用の状態のみを持つ
	bool	playing = false;
	bool	paused = false;

	// pos/rotate/scale から4x4行列を生成(ギズモ表示用)
	DirectX::SimpleMath::Matrix GetMatrix() const;

	bool IsPlaying() const { return playing; }
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エフェクト配置エディタ本体(GPUパーティクル版)
//	KdDebugGUI::GuiProcess() の中(MapEditor::Update()と同じ場所)から
//	Update() を呼び出して使用する。
//	マップ配置エフェクトの描画は3D描画パス側(半透明描画のタイミング)から
//	DrawPreviewParticles() を呼び出して行う(ゲームカメラ・メインシーンのRTに合成される)。
//
//	これとは別に、選択中の1エフェクトだけを専用カメラ・専用のオフスクリーンバッファに
//	描画する「Effect Preview」ウィンドウを持つ。EditorViewport(ゲーム画面をオフスクリーンに
//	描いてImGui::Imageで表示するクラス)と全く同じパターンを踏襲しており、ゲームのメインシーン・
//	ゲームカメラとは完全に独立している。
//	・RenderPreviewViewport() ： 3D描画パス側から、メインシーン描画・DrawPreviewParticles()とは
//	  別に1回呼ぶ(EditorViewport::BeginSceneDraw()と同様、呼び出し前後でRT/ビューポート/
//	  カメラ定数バッファを退避・復元するため、メインシーンの描画状態には一切影響しない)
//	・DrawPreviewWindow()     ： ImGui描画パス側から呼ぶ(Update()内で自動的に呼ばれる)
//
//	※ ImGuizmo::BeginFrame() はMapEditor::Update()側で呼ばれている前提のため、
//	  ここでは呼ばない(1フレームに複数回呼ぶと内部状態がおかしくなるため)
//
//	保存/読み込みはEffectDataLoader(EffectDispatcherと共有するJSONスキーマ)経由で行う。
//	鍔迫り合いの火花(WeaponClashParry/WeaponClashBlock)も専用データ型は持たず、
//	m_objects内の通常のEffectObjectとして扱われる(GPUParticleParams::Layers参照)。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class EffectEditor
{
public:

	// 毎フレームの更新(ImGuiウィンドウ + ギズモ + プレビューのシミュレーション更新)
	void Update();

	// プレビュー中のGPUパーティクルを描画する(マップ配置分。ゲームカメラのシーンに合成される)
	// passと一致するDrawPassのエフェクトだけが実際に描画される(EffectDispatcher::Draw()と同じ規約)
	// ※3D描画パスの最後、DrawLit・DrawBloomそれぞれから、対応するpassで1回ずつ呼び出す事
	//   (KdGPUParticle::Drawと同じ制約：半透明描画のタイミングであること)
	void DrawPreviewParticles(ParticleDrawPass pass);

	// 選択中の1エフェクトだけを、専用カメラでプレビュー専用のオフスクリーンバッファへ描画する
	// ※3D描画パス側(メインシーンの描画・DrawPreviewParticles()とは別のタイミング)から
	//   フレームに1回呼ぶこと。呼び出し場所はEditorViewport::BeginSceneDraw()と同じ並びを想定
	void RenderPreviewViewport();

	// 配置済みエフェクト一覧の取得
	const std::vector<EffectObject>& GetObjects() const { return m_objects; }

	// ループ設定(Continuous、または再発生ありのBurst)の配置エフェクトを一括再生/全プレビュー停止する
	//	ゲーム実行開始時・終了時(EditorViewportの表示切替タイミング等)から呼ぶ想定
	void PlayAllLooping();
	void StopAllPreview();

	// 配置済みの全エフェクトをプレビュー再生する(ループ設定に関わらず全て)
	//	「Effect Editor」ウィンドウの Play All ボタンから呼ばれる
	void PlayAllPreview();

private:

	void DrawMainMenu();
	void DrawHierarchy();
	void DrawInspector();
	void DrawGizmo();
	void DrawTexturePicker();

	// 選択中エフェクトのプレビュー専用ウィンドウ(RenderPreviewViewport()が描いた絵を表示する)
	void DrawPreviewWindow();

	void AddObject();
	void RemoveSelected();

	// nameに一致するEffectObjectをm_objectsから探す(無ければnullptr)。
	// DrawWeaponClashTestPanel()から、"WeaponClashParry"/"WeaponClashBlock"を引く為に使う
	EffectObject* FindObjectByName(const std::string& name);

	void Save(const std::string& path);
	void Load(const std::string& path);

	// マップデータ(JSON)の外部変更を検知して自動リロードする
	void CheckHotReload();

	// テクスチャ用アセットディレクトリ以下を走査して画像ファイル一覧を更新する
	void RefreshTextureFileList();

	// プレビュー再生の開始/停止/一時停止
	void PlayPreview(EffectObject& obj);
	void StopPreview(EffectObject& obj);
	void SetPreviewPause(EffectObject& obj, bool pause);

	// 再生中プレビューの発生・シミュレーションを1フレームぶん進める
	void UpdatePreview(EffectObject& obj, float deltaTime);

	std::vector<EffectObject>	m_objects;
	int							m_selected = -1;

	// テクスチャ取得(EffectInstance用。EffectDispatcherと共通の実装)
	KdAssetsTextureProvider		m_textureProvider;

	ImGuizmo::OPERATION			m_operation = ImGuizmo::TRANSLATE;
	ImGuizmo::MODE				m_mode = ImGuizmo::WORLD;

	bool	m_useSnap = false;
	float	m_snapValue[3] = { 1.0f, 1.0f, 1.0f };

	char	m_filePathBuf[260] = "Asset/Data/Game/effectmap.json";

	// テクスチャアセット一覧
	std::vector<std::string>	m_textureFileList;
	bool						m_textureListLoaded = false;
	bool						m_autoPreviewOnSelect = true;	// Assetsで選択した瞬間にプレビュー再生するか

	// ホットリロード関連
	bool		m_autoReload = true;
	float		m_reloadCheckTimer = 0.0f;
	FILETIME	m_lastWriteTime = {};

	//=====================================================
	// Effect Preview 専用ビューポート
	//	EditorViewport(ゲーム画面をオフスクリーン→ImGui::Imageで表示するクラス)と
	//	全く同じパターンを踏襲した、選択中エフェクト専用のミニビューポート。
	//	ゲームのメインシーン・ゲームカメラとは完全に独立している。
	//=====================================================
	struct PreviewViewport
	{
		std::shared_ptr<KdTexture>	Color;	// オフスクリーンのカラーバッファ
		std::shared_ptr<KdTexture>	Depth;	// オフスクリーンのZバッファ

		int		Width = 0;
		int		Height = 0;

		ImVec2	ScreenPos = { 0,0 };	// ウィンドウ内、画像の左上スクリーン座標(将来ギズモ等を出す場合用)
		ImVec2	ScreenSize = { 0,0 };	// ウィンドウ内、画像の表示サイズ

		// ウィンドウの表示サイズに合わせてオフスクリーンバッファを作り直す(サイズ据え置きなら何もしない)
		void Resize(int w, int h);
	};

	// プレビュー専用の簡易オービットカメラ。選択中エフェクトのpos(ワールド座標)を注視点とする
	struct PreviewCamera
	{
		float Distance = 3.0f;
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
	EffectEditor();
	~EffectEditor() {}

public:
	static EffectEditor& Instance() {
		static EffectEditor instance;
		return instance;
	}
};