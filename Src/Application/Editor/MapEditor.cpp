#include "../main.h"

#include "MapEditor.h"
#include "EditorViewport.h"

#include "imgui_internal.h"

#include <filesystem>
#include <commdlg.h>	
#pragma comment(lib, "comdlg32.lib")
#include "nlohmann/json.hpp"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// pos/rotate(degree)/scale から行列を作る
// ImGuizmoの内部フォーマット(float[16])はDirectXの行列メモリレイアウトと互換のため
// そのままSimpleMath::Matrixへコピーできる
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
DirectX::SimpleMath::Matrix MapObject::GetMatrix() const
{
	float m[16];
	ImGuizmo::RecomposeMatrixFromComponents(&pos.x, &rotate.x, &scale.x, m);

	DirectX::SimpleMath::Matrix mat;
	memcpy(&mat, m, sizeof(float) * 16);
	return mat;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// MapEditor専用ドックスペースの初期レイアウト
//	左：Hierarchy / 中央：Inspector・右：Map Preview / 下：Map Editor(メニュー) + Assets(タブ)
//	EffectEditorのSetupEffectDockLayout()と同じ配分・同じ考え方
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static void SetupMapDockLayout(ImGuiID dockspaceId, const ImVec2& size)
{
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceId, size);

	ImGuiID center = dockspaceId;

	// 左：Hierarchy(幅30%)
	ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.30f, nullptr, &center);

	// 下：Map Editor(メニュー) + Assets(タブ)、高さ35%
	ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.35f, nullptr, &center);

	// 残った中央を Inspector(左) / Map Preview(右) に分割
	ImGuiID preview = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.45f, nullptr, &center);

	ImGui::DockBuilderDockWindow("Hierarchy", left);
	ImGui::DockBuilderDockWindow("Inspector", center);	// 残った中央上
	ImGui::DockBuilderDockWindow("Map Preview", preview);
	ImGui::DockBuilderDockWindow("Assets", bottom);
	ImGui::DockBuilderDockWindow("Map Editor", bottom);	// Assetsとタブ化

	ImGui::DockBuilderFinish(dockspaceId);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 毎フレーム更新
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::Update()
{
	ImGuizmo::BeginFrame();

	// ギズモ操作中でなければショートカットキーを受け付ける
	if (!ImGuizmo::IsUsing())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_1)) m_operation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_2)) m_operation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_3)) m_operation = ImGuizmo::SCALE;
	}

	// マップデータの外部変更検知(ホットリロード)
	CheckHotReload();

	// マップエディタ専用のコンテナウィンドウ
	//	メインビューポートの右外側に初期配置することで、
	//	マルチビューポート機能により起動時から「別ウィンドウ」として分離表示される
	//	(EffectEditorの"Effect Editor Window"と同じ考え方)
	ImGuiViewport* mainViewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos(
		ImVec2(mainViewport->Pos.x + mainViewport->Size.x + 20.0f, mainViewport->Pos.y),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(900.0f, 700.0f), ImGuiCond_FirstUseEver);

	ImGui::Begin("Map Editor Window");
	{
		ImGuiID mapDockId = ImGui::GetID("MapDockSpace");

		if (ImGui::DockBuilderGetNode(mapDockId) == nullptr)
		{
			SetupMapDockLayout(mapDockId, ImGui::GetContentRegionAvail());
		}

		ImGui::DockSpace(mapDockId, ImVec2(0, 0));
	}
	ImGui::End();

	DrawMainMenu();
	DrawHierarchy();
	DrawInspector();
	DrawAssetPicker();
	DrawPreviewWindow();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// メニュー(セーブ/ロード)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::DrawMainMenu()
{
	ImGui::Begin("Map Editor");

	ImGui::InputText("Path", m_filePathBuf, sizeof(m_filePathBuf));

	if (ImGui::Button("Save")) { Save(m_filePathBuf); }
	ImGui::SameLine();
	if (ImGui::Button("Load")) { Load(m_filePathBuf); }
	ImGui::SameLine();
	ImGui::Checkbox("Auto Reload", &m_autoReload);

	ImGui::Text("Objects : %d", (int)m_objects.size());

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 階層ウィンドウ(オブジェクト一覧・追加/削除)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::DrawHierarchy()
{
	ImGui::Begin("Hierarchy");

	if (ImGui::Button("+ Add"))
	{
		AddObject();
	}
	ImGui::SameLine();
	if (ImGui::Button("- Remove"))
	{
		RemoveSelected();
	}

	ImGui::Separator();

	for (int i = 0; i < (int)m_objects.size(); i++)
	{
		bool isSelected = (m_selected == i);

		std::string label = m_objects[i].name + "##" + std::to_string(i);
		if (ImGui::Selectable(label.c_str(), isSelected))
		{
			m_selected = i;
		}
	}

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// インスペクターウィンドウ(選択中オブジェクトのTransform + ギズモ操作モード)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::DrawInspector()
{
	ImGui::Begin("Inspector");

	if (m_selected < 0 || m_selected >= (int)m_objects.size())
	{
		ImGui::TextDisabled("オブジェクトが選択されていません");
		ImGui::End();
		return;
	}

	MapObject& obj = m_objects[m_selected];

	char nameBuf[128];
	strcpy_s(nameBuf, obj.name.c_str());
	if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
	{
		obj.name = nameBuf;
	}

	ImGui::DragFloat3("Position", &obj.pos.x, 0.1f);
	ImGui::DragFloat3("Rotation", &obj.rotate.x, 1.0f);
	ImGui::DragFloat3("Scale", &obj.scale.x, 0.1f, 0.01f, 100.0f);

	ImGui::Separator();
	ImGui::Text("Gizmo Operation");

	if (ImGui::RadioButton("Translate(1)", m_operation == ImGuizmo::TRANSLATE)) m_operation = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate(2)", m_operation == ImGuizmo::ROTATE)) m_operation = ImGuizmo::ROTATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale(3)", m_operation == ImGuizmo::SCALE)) m_operation = ImGuizmo::SCALE;

	if (m_operation != ImGuizmo::SCALE)
	{
		if (ImGui::RadioButton("World", m_mode == ImGuizmo::WORLD)) m_mode = ImGuizmo::WORLD;
		ImGui::SameLine();
		if (ImGui::RadioButton("Local", m_mode == ImGuizmo::LOCAL)) m_mode = ImGuizmo::LOCAL;
	}

	ImGui::Checkbox("Snap", &m_useSnap);
	if (m_useSnap)
	{
		if (m_operation == ImGuizmo::TRANSLATE)
			ImGui::DragFloat3("SnapValue", m_snapValue, 0.1f);
		else
			ImGui::DragFloat("SnapValue", m_snapValue, 0.5f);
	}

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ギズモ描画・操作
//	KdShaderManager の カメラCB(mView / mProjection) を使用
//	※メンバ名はプロジェクト側の実際の型に合わせて調整してください
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::DrawGizmo()
{
	if (m_selected < 0 || m_selected >= (int)m_objects.size()) return;
	if (m_previewViewport.Width <= 0 || m_previewViewport.Height <= 0) return;

	ImGuizmo::SetOrthographic(false);

	// この関数はDrawPreviewWindow()の中、「Map Preview」ウィンドウがまだアクティブな
	// (Begin〜Endの)間に呼ばれる想定。引数無しのSetDrawlist()はその時点で
	// ImGuiが認識している現在のウィンドウのdrawlistを拾うので、これで自動的に
	// 「Map Preview」ウィンドウ上に(=Sceneウィンドウではなくプレビュー画面上に)描画される
	ImGuizmo::SetDrawlist();

	// Sceneウィンドウ全体ではなく、Map Previewウィンドウ内の画像表示範囲を基準にする
	const ImVec2& rectPos = m_previewViewport.ScreenPos;
	const ImVec2& rectSize = m_previewViewport.ScreenSize;
	ImGuizmo::SetRect(rectPos.x, rectPos.y, rectSize.x, rectSize.y);

	// カメラもゲームカメラ(KdShaderManagerのカメラCB)ではなく、
	// RenderPreviewViewport()で実際にプレビュー画面を描いた時と同じ
	// プレビュー専用カメラ(m_previewCamera)の行列を使う
	DirectX::SimpleMath::Matrix view = m_previewCamera.GetView(GetPreviewTarget());
	DirectX::SimpleMath::Matrix proj = m_previewCamera.GetProj(
		(float)m_previewViewport.Width / (float)m_previewViewport.Height);

	MapObject& obj = m_objects[m_selected];

	float matrix[16];
	ImGuizmo::RecomposeMatrixFromComponents(&obj.pos.x, &obj.rotate.x, &obj.scale.x, matrix);

	ImGuizmo::Manipulate(
		reinterpret_cast<const float*>(&view),
		reinterpret_cast<const float*>(&proj),
		m_operation, m_mode, matrix,
		nullptr,
		m_useSnap ? m_snapValue : nullptr);

	if (ImGuizmo::IsUsing())
	{
		ImGuizmo::DecomposeMatrixToComponents(matrix, &obj.pos.x, &obj.rotate.x, &obj.scale.x);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アセット(モデル)選択ウィンドウ
//	Asset/Model 以下の .gltf/.glb を一覧表示し、選択中オブジェクトに割り当てる
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::DrawAssetPicker()
{
	ImGui::Begin("Assets");

	// 初回のみ登録済みリストを読み込む
	if (!m_assetListLoaded)
	{
		LoadModelRegistry(m_registryPathBuf);
		m_assetListLoaded = true;
	}

	if (ImGui::Button("+ Add File..."))
	{
		AddModelViaFileDialog();
	}
	ImGui::SameLine();
	if (ImGui::Button("- Remove") && m_selectedAsset >= 0)
	{
		RemoveRegisteredModel(m_selectedAsset);
	}

	ImGui::Separator();

	if (m_modelFileList.empty())
	{
		ImGui::TextDisabled("登録されたモデルがありません。「+ Add File...」から追加してください");
	}

	for (int i = 0; i < (int)m_modelFileList.size(); i++)
	{
		const std::string& path = m_modelFileList[i];

		bool isSelected = (m_selectedAsset == i);
		if (ImGui::Selectable(path.c_str(), isSelected))
		{
			m_selectedAsset = i;

			// オブジェクトが選択中なら、クリックしたアセットをそのまま割り当てる
			if (m_selected >= 0 && m_selected < (int)m_objects.size())
			{
				m_objects[m_selected].SetModel(kAssetsFilePath + path);
			}
		}
	}

	ImGui::Separator();

	if (m_selected < 0 || m_selected >= (int)m_objects.size())
	{
		ImGui::TextDisabled("オブジェクトを選択してください");
		ImGui::End();
		return;
	}

	MapObject& obj = m_objects[m_selected];
	ImGui::Text("Current : %s", obj.modelPath.empty() ? "(None)" : obj.modelPath.c_str());

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Asset/Model 以下を走査してモデルファイル一覧を更新する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::LoadModelRegistry(const std::string& path)
{
	m_modelFileList.clear();

	nlohmann::json j;
	if (!JsonLoader::Load(path, j))
	{
		// 未作成(初回起動)/壊れたJSON、いずれもリストが空のまま始まる
		return;
	}

	try
	{
		for (auto& e : j)
		{
			m_modelFileList.push_back(e.get<std::string>());
		}
	}
	catch (const nlohmann::json::exception&)
	{
		m_modelFileList.clear();
		KdDebugGUI::Instance().AddLog("MapEditor: アセット一覧の読み込みに失敗 %s\n", path.c_str());
	}
}

void MapEditor::SaveModelRegistry(const std::string& path)
{
	nlohmann::json j = m_modelFileList;

	if (!JsonLoader::Save(path, j))
	{
		KdDebugGUI::Instance().AddLog("MapEditor: アセット一覧の保存に失敗 %s\n", path.c_str());
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Windowsのファイル選択ダイアログでモデルファイルを1つ登録する
//	選択されたファイルは実行ディレクトリからの相対パスに変換して登録・保存する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::AddModelViaFileDialog()
{
	namespace fs = std::filesystem;

	fs::path savedCurrentDir = fs::current_path();

	char fileBuf[MAX_PATH] = {};

	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = Application::Instance().GetWindowHandle();
	ofn.lpstrFilter = "Model Files (*.gltf;*.glb)\0*.gltf;*.glb\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = fileBuf;
	ofn.nMaxFile = sizeof(fileBuf);
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	bool result = GetOpenFileNameA(&ofn);

	fs::current_path(savedCurrentDir);

	if (!result)
	{
		return;	// キャンセルされた
	}

	std::string relativePath;
	try
	{
		fs::path full = fs::absolute(fileBuf);
		fs::path base = fs::absolute(kAssetsFilePath);
		relativePath = fs::relative(full, base).generic_string();
	}
	catch (...)
	{
		relativePath = fileBuf;	// 変換に失敗した場合はフルパスのまま登録
	}

	// 既に登録済みなら何もしない
	for (auto& p : m_modelFileList)
	{
		if (p == relativePath) return;
	}

	m_modelFileList.push_back(relativePath);
	SaveModelRegistry(m_registryPathBuf);
}

void MapEditor::RemoveRegisteredModel(int index)
{
	if (index < 0 || index >= (int)m_modelFileList.size()) return;

	m_modelFileList.erase(m_modelFileList.begin() + index);
	m_selectedAsset = -1;

	SaveModelRegistry(m_registryPathBuf);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 配置済みオブジェクトの実描画(共通部分)
//	現在のKdShaderManagerのカメラCBに対して描画するだけの処理。
//	どのカメラ(ゲームカメラ/プレビュー専用カメラ)が設定されているかは呼び出し側の責任とする
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::DrawObjects()
{
	for (auto& obj : m_objects)
	{
		// モデル未割り当てのオブジェクトはスキップ
		if (!obj.modelWork.IsEnable()) continue;

		// ノード行列の再計算が必要なら計算(SetModelData直後など)
		if (obj.modelWork.NeedCalcNodeMatrices())
		{
			obj.modelWork.CalcNodeMatrices();
		}

		KdShaderManager::Instance().m_StandardShader.DrawModel(obj.modelWork, obj.GetMatrix());
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// プレビュー用カメラの注視点：選択中オブジェクトがあればその位置。
// 未選択時は原点固定ではなく、配置済みオブジェクト全体の重心を注視点にする
// (原点固定のままだと、マップが原点から離れた場所に作られている場合に
//  何も選択していない状態でプレビューを開くと画角内に何も入らず「何も映らない」ように見える)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
DirectX::SimpleMath::Vector3 MapEditor::GetPreviewTarget() const
{
	// ギズモ操作中は注視点を固定する(操作開始時点にキャッシュした値をそのまま返す)
	if (ImGuizmo::IsUsing())
	{
		return m_previewTargetCache;
	}

	DirectX::SimpleMath::Vector3 target = { 0,0,0 };

	if (m_selected >= 0 && m_selected < (int)m_objects.size())
	{
		target = m_objects[m_selected].pos;
	}
	else if (!m_objects.empty())
	{
		DirectX::SimpleMath::Vector3 sum = { 0,0,0 };
		for (auto& obj : m_objects) { sum += obj.pos; }
		target = sum / (float)m_objects.size();
	}

	m_previewTargetCache = target;
	return target;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 配置済みオブジェクトの実描画
//	SceneManager::Draw() など、3D描画パスから呼び出すこと
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::DrawPlacedObjects()
{
	DrawObjects();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップ全体を、専用カメラ・専用オフスクリーンバッファへ描画する
//	EffectEditor::RenderPreviewViewport()と同じ構成：
//	現在のRT/ビューポート/カメラCBを退避し、プレビュー用に差し替えて描画した後、元に戻す
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::RenderPreviewViewport()
{
	// ウィンドウが一度も開かれておらずサイズが確定していない場合は何もしない
	if (!m_previewViewport.Color || !m_previewViewport.Depth) return;
	if (m_previewViewport.Width <= 0 || m_previewViewport.Height <= 0) return;

	ID3D11DeviceContext* context = KdDirect3D::Instance().WorkDevContext();

	// 退避
	KdShaderManager::cbCamera savedCamera = KdShaderManager::Instance().GetCameraCB();

	ID3D11RenderTargetView* savedRTV = nullptr;
	ID3D11DepthStencilView* savedDSV = nullptr;
	context->OMGetRenderTargets(1, &savedRTV, &savedDSV);

	UINT savedVPNum = 1;
	D3D11_VIEWPORT savedVP = {};
	context->RSGetViewports(&savedVPNum, &savedVP);

	// プレビュー用バッファへ切り替え・クリア
	ID3D11RenderTargetView* rtvs[] = { m_previewViewport.Color->WorkRTView() };
	context->OMSetRenderTargets(1, rtvs, m_previewViewport.Depth->WorkDSView());

	static const float clearColor[4] = { 0.1f, 0.1f, 0.12f, 1.0f };
	context->ClearRenderTargetView(m_previewViewport.Color->WorkRTView(), clearColor);
	context->ClearDepthStencilView(m_previewViewport.Depth->WorkDSView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	D3D11_VIEWPORT vp = {};
	vp.Width = (float)m_previewViewport.Width;
	vp.Height = (float)m_previewViewport.Height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	context->RSSetViewports(1, &vp);

	// プレビュー用カメラの注視点(選択中オブジェクト、または配置済み全体の重心)
	DirectX::SimpleMath::Vector3 target = GetPreviewTarget();

	DirectX::SimpleMath::Matrix view = m_previewCamera.GetView(target);
	DirectX::SimpleMath::Matrix proj = m_previewCamera.GetProj(
		(float)m_previewViewport.Width / (float)m_previewViewport.Height);

	KdShaderManager::Instance().WriteCBCamera(view.Invert(), proj);

	// マップ全体(配置済みオブジェクトすべて)を描画する
	//	KdStandardShader::DrawModel()自体はVS/PS/InputLayout/サンプラーステートをセットしない
	//	(それらはBeginLit()側の責務)。DrawPlacedObjects()はSceneManager::Draw()内の
	//	BeginLit()〜EndLit()ブラケットの中で呼ばれる想定だが、こちらはメインシーンの描画とは
	//	別タイミングで独立して呼ばれるパスなので、自前でBeginLit()/EndLit()を呼んで
	//	パイプライン状態を保証する
	KdShaderManager::Instance().m_StandardShader.BeginLit();
	DrawObjects();
	KdShaderManager::Instance().m_StandardShader.EndLit();

	// 復元
	KdShaderManager::Instance().WriteCBCamera(savedCamera.mView.Invert(), savedCamera.mProj);

	context->OMSetRenderTargets(1, &savedRTV, savedDSV);
	if (savedRTV) { savedRTV->Release(); }
	if (savedDSV) { savedDSV->Release(); }

	context->RSSetViewports(savedVPNum, &savedVP);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップ全体プレビューウィンドウ(RenderPreviewViewport()が描いた絵を表示する)
//	右ドラッグでオービット回転、ホイールでズーム(EffectEditor::DrawPreviewWindow()と同じ操作感)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::DrawPreviewWindow()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

	// ImGuiはデフォルトで「タイトルバー以外の空き領域をドラッグしてもウィンドウが動く」ため、
	// ImGui::Image()自体はクリックを捕捉しない(ボタンではない)ので、
	// 何もしないとギズモ操作や右ドラッグオービットより先にウィンドウ移動が発生してしまう。
	// このウィンドウ内ではドラッグ操作をギズモ/カメラ操作専用にしたいので、NoMoveを付ける
	// (タブ部分からのドッキング操作には影響しない)
	ImGui::Begin("Map Preview", nullptr, ImGuiWindowFlags_NoMove);

	ImVec2 regionSize = ImGui::GetContentRegionAvail();

	// ウィンドウサイズが変わったらオフスクリーンバッファを作り直す
	if (regionSize.x >= 1.0f && regionSize.y >= 1.0f)
	{
		m_previewViewport.Resize((int)regionSize.x, (int)regionSize.y);
	}

	if (m_previewViewport.Color)
	{
		m_previewViewport.ScreenPos = ImGui::GetCursorScreenPos();
		m_previewViewport.ScreenSize = regionSize;

		ImGui::Image((ImTextureID)m_previewViewport.Color->WorkSRView(), regionSize);

		// 右ドラッグ：オービット回転、ホイール：ズーム
		if (ImGui::IsItemHovered())
		{
			ImGuiIO& io = ImGui::GetIO();

			if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
			{
				m_previewCamera.Yaw -= io.MouseDelta.x * 0.01f;
				m_previewCamera.Pitch += io.MouseDelta.y * 0.01f;
				m_previewCamera.Pitch = std::clamp(m_previewCamera.Pitch, -1.5f, 1.5f);
			}

			if (io.MouseWheel != 0.0f)
			{
				m_previewCamera.Distance -= io.MouseWheel * 0.5f;
				m_previewCamera.Distance = std::clamp(m_previewCamera.Distance, 0.2f, 200.0f);
			}
		}

		// ギズモは「Map Preview」ウィンドウがまだアクティブな(Begin〜Endの)間に呼ぶことで、
		// ImGuizmo::SetDrawlist()が自動的にこのウィンドウのdrawlistを拾ってくれる
		// (Sceneウィンドウではなくプレビュー画面上で操作できるようにするため)
		DrawGizmo();
	}

	if (m_objects.empty())
	{
		ImGui::SetCursorPos(ImVec2(10, 10));
		ImGui::TextDisabled("配置されたオブジェクトがありません");
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

void MapEditor::PreviewViewport::Resize(int w, int h)
{
	if (w <= 0 || h <= 0) return;

	// サイズが変わっていなければ作り直さない
	if (w == Width && h == Height && Color && Depth) return;

	Width = w;
	Height = h;

	// ----- カラーバッファ -----
	{
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.Width = (UINT)w;
		desc.Height = (UINT)h;
		desc.CPUAccessFlags = 0;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		Color = std::make_shared<KdTexture>();
		Color->Create(desc);
	}

	// ----- Zバッファ -----
	{
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		desc.Width = (UINT)w;
		desc.Height = (UINT)h;
		desc.CPUAccessFlags = 0;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		Depth = std::make_shared<KdTexture>();
		Depth->Create(desc);
	}
}

DirectX::SimpleMath::Matrix MapEditor::PreviewCamera::GetView(const DirectX::SimpleMath::Vector3& target) const
{
	using namespace DirectX::SimpleMath;

	float cosPitch = cosf(Pitch);
	Vector3 offset(
		Distance * cosPitch * sinf(Yaw),
		Distance * sinf(Pitch),
		Distance * cosPitch * cosf(Yaw));

	Vector3 eye = target + offset;
	return Matrix::CreateLookAt(eye, target, Vector3::Up);
}

DirectX::SimpleMath::Matrix MapEditor::PreviewCamera::GetProj(float aspect) const
{
	return DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(
		DirectX::XMConvertToRadians(45.0f), aspect, 0.05f, 500.0f);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// オブジェクトの追加/削除
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::AddObject()
{
	MapObject obj;
	obj.name = "Object" + std::to_string(m_objects.size());
	m_objects.push_back(obj);
	m_selected = (int)m_objects.size() - 1;
}

void MapEditor::RemoveSelected()
{
	if (m_selected < 0 || m_selected >= (int)m_objects.size()) return;

	m_objects.erase(m_objects.begin() + m_selected);
	m_selected = -1;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// セーブ/ロード(仮実装：nlohmann/json使用)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::Save(const std::string& path)
{
	nlohmann::json j;

	for (auto& obj : m_objects)
	{
		j.push_back({
			{ "name",   obj.name },
			{ "pos",    { obj.pos.x, obj.pos.y, obj.pos.z } },
			{ "rotate", { obj.rotate.x, obj.rotate.y, obj.rotate.z } },
			{ "scale",  { obj.scale.x, obj.scale.y, obj.scale.z } },
			{ "model",  obj.modelPath }
			});
	}

	if (!JsonLoader::Save(path, j))
	{
		KdDebugGUI::Instance().AddLog("MapEditor: 保存に失敗 %s\n", path.c_str());
		return;
	}

	// 自分で保存した直後のタイムスタンプを覚えておき、
	// 直後のCheckHotReload()で「外部変更」と誤検知して再ロードしないようにする
	FILETIME writeTime;
	if (JsonLoader::GetLastWriteTime(path, writeTime))
	{
		m_lastWriteTime = writeTime;
	}

	KdDebugGUI::Instance().AddLog("MapEditor: 保存しました %s\n", path.c_str());
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップデータ(JSON)の更新日時をポーリングし、外部から変更されていたら自動で再読み込みする
//	・エディタ外(テキストエディタ、Git、別ツール等)でJSONを直接編集した場合の即時反映用
//	・0.5秒間隔でチェックするため、毎フレームファイルI/Oは発生しない
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::CheckHotReload()
{
	if (!m_autoReload) return;

	// ポーリング間隔を空ける(毎フレームGetFileAttributesExを呼ばない)
	m_reloadCheckTimer += Application::Instance().GetDeltaTime();
	if (m_reloadCheckTimer < 0.5f) return;
	m_reloadCheckTimer = 0.0f;

	FILETIME writeTime;
	if (!JsonLoader::GetLastWriteTime(m_filePathBuf, writeTime))
	{
		// ファイルが存在しない等 → 何もしない
		return;
	}

	// 初回チェック時は基準時刻を記録するだけ(起動直後の誤リロード防止)
	if (m_lastWriteTime.dwLowDateTime == 0 && m_lastWriteTime.dwHighDateTime == 0)
	{
		m_lastWriteTime = writeTime;
		return;
	}

	if (CompareFileTime(&writeTime, &m_lastWriteTime) == 0)
	{
		// 更新なし
		return;
	}

	m_lastWriteTime = writeTime;

	// 選択状態はできる範囲で維持する
	int keepSelected = m_selected;

	Load(m_filePathBuf);

	if (keepSelected >= 0 && keepSelected < (int)m_objects.size())
	{
		m_selected = keepSelected;
	}

	KdDebugGUI::Instance().AddLog("MapEditor: 外部変更を検知し自動リロードしました (%s)\n", m_filePathBuf);
}

void MapEditor::Load(const std::string& path)
{
	nlohmann::json j;
	if (!JsonLoader::Load(path, j))
	{
		// ファイルが無い/JSONとして壊れている、いずれもここで弾かれる(例外は投げない)
		KdDebugGUI::Instance().AddLog("MapEditor: 読み込み失敗 %s\n", path.c_str());
		return;
	}

	std::vector<MapObject> loaded;

	try
	{
		for (auto& e : j)
		{
			MapObject obj;
			obj.name = e.at("name").get<std::string>();
			obj.pos = { e.at("pos")[0],    e.at("pos")[1],    e.at("pos")[2] };
			obj.rotate = { e.at("rotate")[0], e.at("rotate")[1], e.at("rotate")[2] };
			obj.scale = { e.at("scale")[0],  e.at("scale")[1],  e.at("scale")[2] };

			// "model"キーは旧バージョンのJSONには存在しないため value() でデフォルト値対応
			std::string modelPath = e.value("model", std::string());
			if (!modelPath.empty())
			{
				obj.SetModel(modelPath);	// ここで実際のモデル読み込みが走る
			}

			loaded.push_back(std::move(obj));
		}
	}
	catch (const nlohmann::json::exception&)
	{
		// 想定外のスキーマ(キー欠落・型不一致等)。
		// ここで例外を握りつぶし、現在の m_objects には触れずに読み込み失敗として扱う
		KdDebugGUI::Instance().AddLog("MapEditor: 読み込み失敗(不正なデータ形式) %s\n", path.c_str());
		return;
	}

	m_objects = std::move(loaded);
	m_selected = -1;
	KdDebugGUI::Instance().AddLog("MapEditor: 読み込みました %s\n", path.c_str());
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// コンストラクタ：起動時に既存のマップデータ(m_filePathBuf)を自動ロードする
// (EffectEditorと同じ挙動。未作成ならJsonLoader::Load側で失敗し、空のまま始まる)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
MapEditor::MapEditor()
{
	Load(m_filePathBuf);
	LoadModelRegistry(m_registryPathBuf);
}