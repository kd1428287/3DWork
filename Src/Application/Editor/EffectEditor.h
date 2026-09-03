#pragma once

// ※ ImGui / DirectXTK(SimpleMath) / KdResourceFactory・KdTexture は
//    既存のPCH等で読み込まれている前提です。読み込まれていない場合は
//    実際の配置に合わせてincludeを追加してください。
#include "ImGuizmo.h"
#include "../../Framework/Shader/GPUParticle/KdGPUParticle.h"	// 実際の配置パスに合わせて調整

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// パーティクルのブレンドモード(将来的な描画方式切り替え用)
// ※現状のKdGPUParticle::Draw()は加算合成(KdBlendState::Add)固定のため、
//   ここで選んだ値を実際に反映するにはKdGPUParticle::Draw()側に
//   ブレンドモード引数を追加する対応が別途必要。
//   エディタ側は先にデータとして保持・保存できるようにしておく。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
enum class KdParticleBlendMode
{
	Add,	// 加算合成(現状のKdGPUParticleのデフォルト挙動)
	Alpha,	// 半透明合成
};

// パーティクルの発生方式
enum class KdParticleEmitMode
{
	Burst,		// EmitCount個を一度に発生。EmitInterval(秒)ごとに繰り返す(0以下なら再生開始時の1回のみ)
	Continuous,	// 毎秒EmitRate個ずつ、再生中ずっと発生させ続ける
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 1エフェクト分のパーティクル発生パラメータ
//	KdGPUParticle::EmitParameterの内容に加え、発生の仕方(Burst/Continuous)や
//	見た目(テクスチャ・ブレンドモード)などエディタ/JSONで保存したい情報をまとめたもの。
//	KdGPUParticle::Init()自体はここには含まない(MaxParticleNumのみ保持し、
//	実際のInit呼び出しはEffectEditor側でプレビュー開始時に行う)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct GPUParticleParams
{
	UINT	MaxParticleNum = 500;	// KdGPUParticle::Init()に渡す同時最大パーティクル数

	KdParticleEmitMode	EmitMode = KdParticleEmitMode::Burst;
	int		EmitCount = 30;			// Burst：1回あたりの発生数
	float	EmitInterval = 0.0f;	// Burst：再発生までの間隔(秒)。0以下なら自動リピートしない
	float	EmitRate = 30.0f;		// Continuous：1秒あたりの発生数

	DirectX::SimpleMath::Vector3	VelocityMin = { -1,-1,-1 };
	DirectX::SimpleMath::Vector3	VelocityMax = { 1, 1, 1 };

	float	SizeMin = 0.1f;
	float	SizeMax = 0.3f;

	float	LifeMin = 0.5f;
	float	LifeMax = 1.5f;

	DirectX::SimpleMath::Vector4	Color = { 1,1,1,1 };
	DirectX::SimpleMath::Vector3	Gravity = { 0.0f, -0.5f, 0.0f };

	std::string	TexturePath;	// Asset/Textureからの相対パス

	KdParticleBlendMode BlendMode = KdParticleBlendMode::Add;	// TODO: KdGPUParticle::Draw()側の対応待ち

	// 現在の値からKdGPUParticle::Emit()へ渡すEmitParameterを作る(位置は呼び出し側から渡す)
	KdGPUParticle::EmitParameter ToEmitParameter(const DirectX::SimpleMath::Vector3& worldPos) const;

	// EmitInterval>0のBurst、またはContinuousなら「放置していると自動で出続ける」エフェクトとみなす
	bool IsLooping() const;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップに配置する1エフェクト分のデータ
//	「どこに・どんなGPUパーティクルを・どう発生させるか」を保持する。
//	実体のパーティクルバッファ(KdGPUParticle)はプレビュー再生時に遅延生成し、
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
	//--------------------------------------------------
	std::shared_ptr<KdGPUParticle>	previewParticle;	// 再生開始時に必要な容量でInit()して生成
	std::shared_ptr<KdTexture>		previewTexture;		// TexturePathから読み込んだテクスチャのキャッシュ
	UINT	previewCapacity = 0;						// previewParticleが実際にInit()された時のMaxParticleNum

	bool	playing = false;
	bool	paused = false;
	float	emitAccumulator = 0.0f;	// Continuous発生の端数(1個未満)蓄積用
	float	burstTimer = 0.0f;			// Burstモードで次に発生させるまでの残り時間

	// pos/rotate/scale から4x4行列を生成(ギズモ表示用)
	DirectX::SimpleMath::Matrix GetMatrix() const;

	bool IsPlaying() const { return playing; }
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エフェクト配置エディタ本体(GPUパーティクル版)
//	KdDebugGUI::GuiProcess() の中(MapEditor::Update()と同じ場所)から
//	Update() を呼び出して使用する。
//	パーティクル自体の描画は3D描画パス側(半透明描画のタイミング)から
//	DrawPreviewParticles() を呼び出して行う。
//	※ ImGuizmo::BeginFrame() はMapEditor::Update()側で呼ばれている前提のため、
//	  ここでは呼ばない(1フレームに複数回呼ぶと内部状態がおかしくなるため)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class EffectEditor
{
public:

	// 毎フレームの更新(ImGuiウィンドウ + ギズモ + プレビューのシミュレーション更新)
	void Update();

	// プレビュー中のGPUパーティクルを描画する
	// ※3D描画パスの最後、半透明描画のタイミングで呼び出す事(KdGPUParticle::Drawと同じ制約)
	void DrawPreviewParticles();

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

	void AddObject();
	void RemoveSelected();

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

	// obj.params.MaxParticleNumに合わせてpreviewParticleを(必要なら)作り直す
	void EnsurePreviewParticleCapacity(EffectObject& obj);

	// obj.params.TexturePathに合わせてpreviewTextureを(必要なら)読み込み直す
	void EnsurePreviewTexture(EffectObject& obj);

	std::vector<EffectObject>	m_objects;
	int							m_selected = -1;

	ImGuizmo::OPERATION			m_operation = ImGuizmo::TRANSLATE;
	ImGuizmo::MODE				m_mode = ImGuizmo::WORLD;

	bool	m_useSnap = false;
	float	m_snapValue[3] = { 1.0f, 1.0f, 1.0f };

	char	m_filePathBuf[260] = "Asset/Data/effectmap.json";

	// テクスチャアセット一覧
	std::vector<std::string>	m_textureFileList;
	bool						m_textureListLoaded = false;
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