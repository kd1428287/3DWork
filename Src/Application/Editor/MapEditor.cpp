#include "../main.h"

#include "MapEditor.h"
#include "EditorViewport.h"

#include <fstream>
#include <filesystem>
// 未導入の場合はSave/Loadごと削除するか、独自の保存形式に差し替えてください
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

	DrawMainMenu();
	DrawHierarchy();
	DrawInspector();
	DrawAssetPicker();
	DrawGizmo();
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

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();

	// 画面全体ではなく、Sceneウィンドウ内の画像表示範囲を基準にする
	const ImVec2& rectPos = EditorViewport::Instance().GetScreenPos();
	const ImVec2& rectSize = EditorViewport::Instance().GetScreenSize();
	ImGuizmo::SetRect(rectPos.x, rectPos.y, rectSize.x, rectSize.y);

	const auto& cameraCB = KdShaderManager::Instance().GetCameraCB();
	const DirectX::SimpleMath::Matrix& view = cameraCB.mView;
	const DirectX::SimpleMath::Matrix& proj = cameraCB.mProj;

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

	// 初回のみ自動スキャン
	if (!m_assetListLoaded)
	{
		RefreshModelFileList();
		m_assetListLoaded = true;
	}

	if (ImGui::Button("Refresh"))
	{
		RefreshModelFileList();
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
	ImGui::Separator();

	for (auto& path : m_modelFileList)
	{
		bool isSelected = (obj.modelPath == path);
		if (ImGui::Selectable(path.c_str(), isSelected))
		{
			obj.SetModel(path);
		}
	}

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Asset/Model 以下を走査してモデルファイル一覧を更新する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::RefreshModelFileList()
{
	m_modelFileList.clear();

	namespace fs = std::filesystem;

	const std::string root = "Asset/Model";	// ※実際のモデル格納フォルダに合わせて調整

	if (!fs::exists(root)) return;

	for (auto& entry : fs::recursive_directory_iterator(root))
	{
		if (!entry.is_regular_file()) continue;

		std::string ext = entry.path().extension().string();
		if (ext != ".gltf" && ext != ".glb") continue;

		// 表示・JSON保存の一貫性のため区切り文字をスラッシュに統一
		m_modelFileList.push_back(entry.path().generic_string());
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 配置済みオブジェクトの実描画
//	SceneManager::Draw() など、3D描画パスから呼び出すこと
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::DrawPlacedObjects()
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

	std::ofstream ofs(path);
	if (!ofs)
	{
		KdDebugGUI::Instance().AddLog("MapEditor: 保存に失敗 %s\n", path.c_str());
		return;
	}

	ofs << j.dump(2);
	ofs.close();

	// 自分で保存した直後のタイムスタンプを覚えておき、
	// 直後のCheckHotReload()で「外部変更」と誤検知して再ロードしないようにする
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data))
	{
		m_lastWriteTime = data.ftLastWriteTime;
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

	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!GetFileAttributesExA(m_filePathBuf, GetFileExInfoStandard, &data))
	{
		// ファイルが存在しない等 → 何もしない
		return;
	}

	// 初回チェック時は基準時刻を記録するだけ(起動直後の誤リロード防止)
	if (m_lastWriteTime.dwLowDateTime == 0 && m_lastWriteTime.dwHighDateTime == 0)
	{
		m_lastWriteTime = data.ftLastWriteTime;
		return;
	}

	if (CompareFileTime(&data.ftLastWriteTime, &m_lastWriteTime) == 0)
	{
		// 更新なし
		return;
	}

	m_lastWriteTime = data.ftLastWriteTime;

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
	std::ifstream ifs(path);
	if (!ifs)
	{
		KdDebugGUI::Instance().AddLog("MapEditor: 読み込み失敗 %s\n", path.c_str());
		return;
	}

	nlohmann::json j;
	ifs >> j;

	m_objects.clear();

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

		m_objects.push_back(obj);
	}

	m_selected = -1;
	KdDebugGUI::Instance().AddLog("MapEditor: 読み込みました %s\n", path.c_str());
}