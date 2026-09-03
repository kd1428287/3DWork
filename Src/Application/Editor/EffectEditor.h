//#pragma once
//
//#include "ImGuizmo.h"
//#include "../Effect/EffectParams.h"	
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// マップに配置する1エフェクト分のデータ
////	「どこに・どんなGPUパーティクルを・どう発生させるか」を保持する。
////	実体のパーティクルバッファ(KdGPUParticle)はプレビュー再生時に遅延生成し、
////	JSONにはparamsとtransformのみを保存する(プレビュー用の状態は保存しない)
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//struct EffectObject
//{
//	std::string	name = "Effect";
//
//	DirectX::SimpleMath::Vector3	pos = { 0,0,0 };
//	DirectX::SimpleMath::Vector3	rotate = { 0,0,0 };	// 度数法(degree) X,Y,Z ※現状パーティクルの発生方向には未反映(下記TODO参照)
//	DirectX::SimpleMath::Vector3	scale = { 1,1,1 };		// 現状は表示上のギズモ操作用。パーティクルサイズには未反映
//
//	GPUParticleParams	params;
//
//	//--------------------------------------------------
//	// プレビュー再生用の実行時状態(JSONには保存しない)
//	//--------------------------------------------------
//	std::shared_ptr<KdGPUParticle>	previewParticle;	// 再生開始時に必要な容量でInit()して生成
//	std::shared_ptr<KdTexture>		previewTexture;		// TexturePathから読み込んだテクスチャのキャッシュ
//	UINT	previewCapacity = 0;						// previewParticleが実際にInit()された時のMaxParticleNum
//
//	bool	playing = false;
//	bool	paused = false;
//	float	emitAccumulator = 0.0f;	// Continuous発生の端数(1個未満)蓄積用
//	float	burstTimer = 0.0f;			// Burstモードで次に発生させるまでの残り時間
//
//	// pos/rotate/scale から4x4行列を生成(ギズモ表示用)
//	DirectX::SimpleMath::Matrix GetMatrix() const;
//
//	bool IsPlaying() const { return playing; }
//};
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// エフェクト配置エディタ本体(GPUパーティクル版)
////	KdDebugGUI::GuiProcess() の中(MapEditor::Update()と同じ場所)から
////	Update() を呼び出して使用する。
////	パーティクル自体の描画は3D描画パス側(半透明描画のタイミング)から
////	DrawPreviewParticles() を呼び出して行う。
////	※ ImGuizmo::BeginFrame() はMapEditor::Update()側で呼ばれている前提のため、
////	  ここでは呼ばない(1フレームに複数回呼ぶと内部状態がおかしくなるため)
////
////	保存/読み込みはEffectDataLoader(EffectDispatcherと共有するJSONスキーマ)経由で行う。
////	マップ配置エフェクト一覧(m_objects)に加え、鍔迫り合いの火花(m_weaponClash、
////	配置を持たないグローバル設定)も同じJSONファイルにまとめて保存する。
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//class EffectEditor
//{
//public:
//
//	// 毎フレームの更新(ImGuiウィンドウ + ギズモ + プレビューのシミュレーション更新)
//	void Update();
//
//	// プレビュー中のGPUパーティクルを描画する
//	// ※3D描画パスの最後、半透明描画のタイミングで呼び出す事(KdGPUParticle::Drawと同じ制約)
//	void DrawPreviewParticles();
//
//	// 配置済みエフェクト一覧の取得
//	const std::vector<EffectObject>& GetObjects() const { return m_objects; }
//
//	// ループ設定(Continuous、または再発生ありのBurst)の配置エフェクトを一括再生/全プレビュー停止する
//	//	ゲーム実行開始時・終了時(EditorViewportの表示切替タイミング等)から呼ぶ想定
//	void PlayAllLooping();
//	void StopAllPreview();
//
//	// 配置済みの全エフェクトをプレビュー再生する(ループ設定に関わらず全て)
//	//	「Effect Editor」ウィンドウの Play All ボタンから呼ばれる
//	void PlayAllPreview();
//
//private:
//
//	//struct PreviewCamera
//	//{
//	//	DirectX::SimpleMath::Vector3 target = { 0,0,0 };
//	//	float distance = 3.0f;
//	//	float yaw = 0.0f;
//	//	float pitch = 0.3f;
//
//	//	DirectX::SimpleMath::Matrix GetView() const;
//	//	DirectX::SimpleMath::Matrix GetProj(float aspect) const;
//	//	void UpdateFromImGuiInput(); // ウィンドウ内でのドラッグ操作を受け取る
//	//};
//
//	//PreviewCamera m_previewCamera;
//
//	//Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_previewColorTex;
//	//Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_previewRTV;
//	//Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_previewSRV;
//	//Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_previewDepthTex;
//	//Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   m_previewDSV;
//	//UINT m_previewWidth = 512, m_previewHeight = 512;
//
//	//void EnsurePreviewRenderTarget(UINT width, UINT height);
//	//void RenderPreviewScene(); // ← 新規。メイン3D描画とは別に毎フレーム1回呼ぶ
//	//void DrawPreviewWindow();  // ImGui::Image() で m_previewSRV を表示するウィンドウ
//
//	void DrawMainMenu();
//	void DrawHierarchy();
//	void DrawInspector();
//	void DrawGizmo();
//	void DrawTexturePicker();
//	void DrawWeaponClashInspector();
//
//	void AddObject();
//	void RemoveSelected();
//
//	void Save(const std::string& path);
//	void Load(const std::string& path);
//
//	// マップデータ(JSON)の外部変更を検知して自動リロードする
//	void CheckHotReload();
//
//	// テクスチャ用アセットディレクトリ以下を走査して画像ファイル一覧を更新する
//	void RefreshTextureFileList();
//
//	// プレビュー再生の開始/停止/一時停止
//	void PlayPreview(EffectObject& obj);
//	void StopPreview(EffectObject& obj);
//	void SetPreviewPause(EffectObject& obj, bool pause);
//
//	// 再生中プレビューの発生・シミュレーションを1フレームぶん進める
//	void UpdatePreview(EffectObject& obj, float deltaTime);
//
//	// obj.params.MaxParticleNumに合わせてpreviewParticleを(必要なら)作り直す
//	void EnsurePreviewParticleCapacity(EffectObject& obj);
//
//	// obj.params.TexturePathに合わせてpreviewTextureを(必要なら)読み込み直す
//	void EnsurePreviewTexture(EffectObject& obj);
//
//	// m_weaponClash.MaxParticleNumに合わせてm_weaponClashPreviewParticleを(必要なら)作り直す
//	void EnsureWeaponClashPreviewCapacity();
//
//	// 鍔迫り合いの火花を固定方向(+X)でテスト再生する(Weapon Clashインスペクターのボタンから呼ばれる)
//	void TestEmitWeaponClash(bool isParry);
//
//	std::vector<EffectObject>	m_objects;
//	int							m_selected = -1;
//
//	// 鍔迫り合いの火花(配置を持たない単一のグローバル設定)
//	WeaponClashEffectParams			m_weaponClash;
//	std::shared_ptr<KdGPUParticle>	m_weaponClashPreviewParticle;
//	UINT							m_weaponClashPreviewCapacity = 0;
//
//	ImGuizmo::OPERATION			m_operation = ImGuizmo::TRANSLATE;
//	ImGuizmo::MODE				m_mode = ImGuizmo::WORLD;
//
//	bool	m_useSnap = false;
//	float	m_snapValue[3] = { 1.0f, 1.0f, 1.0f };
//
//	char	m_filePathBuf[260] = "Asset/Data/Game/effectmap.json";
//
//	// テクスチャアセット一覧
//	std::vector<std::string>	m_textureFileList;
//	bool						m_textureListLoaded = false;
//	bool						m_autoPreviewOnSelect = true;	// Assetsで選択した瞬間にプレビュー再生するか
//
//	// ホットリロード関連
//	bool		m_autoReload = true;
//	float		m_reloadCheckTimer = 0.0f;
//	FILETIME	m_lastWriteTime = {};
//
//	//=====================================================
//	// シングルトンパターン
//	//=====================================================
//private:
//	EffectEditor() {}
//	~EffectEditor() {}
//
//public:
//	static EffectEditor& Instance() {
//		static EffectEditor instance;
//		return instance;
//	}
//};