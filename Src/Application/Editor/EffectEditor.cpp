#include "../main.h"

#include "EffectEditor.h"
#include "EditorViewport.h"

// DockBuilder系APIを使うために必要
#include "imgui_internal.h"

#include <fstream>
#include <filesystem>
// 未導入の場合はSave/Loadごと削除するか、独自の保存形式に差し替えてください
#include "nlohmann/json.hpp"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// pos/rotate(degree)/scale から行列を作る(MapObjectと同じ仕組み)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
DirectX::SimpleMath::Matrix EffectObject::GetMatrix() const
{
	float m[16];
	ImGuizmo::RecomposeMatrixFromComponents(&pos.x, &rotate.x, &scale.x, m);

	DirectX::SimpleMath::Matrix mat;
	memcpy(&mat, m, sizeof(float) * 16);
	return mat;
}

bool EffectObject::IsPlaying() const
{
	auto sp = wpPlaying.lock();
	return sp && sp->IsPlaying();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// EffectEditor専用ドックスペースの初期レイアウト
//	左：Effect Hierarchy / 中央：Effect Inspector / 下：Effect Editor(メニュー) + Effect Assets(タブ)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static void SetupEffectDockLayout(ImGuiID dockspaceId, const ImVec2& size)
{
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceId, size);

	ImGuiID center = dockspaceId;

	// 左：Effect Hierarchy(幅30%)
	ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.30f, nullptr, &center);

	// 下：Effect Editor(メニュー) + Effect Assets(タブ)、高さ35%
	ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.35f, nullptr, &center);

	ImGui::DockBuilderDockWindow("Effect Hierarchy", left);
	ImGui::DockBuilderDockWindow("Effect Inspector", center);	// 残った中央上
	ImGui::DockBuilderDockWindow("Effect Assets", bottom);
	ImGui::DockBuilderDockWindow("Effect Editor", bottom);	// Effect Assetsとタブ化

	ImGui::DockBuilderFinish(dockspaceId);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 毎フレーム更新
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectEditor::Update()
{
	// ショートカットキー(ImGuizmo::BeginFrame()はMapEditor::Update()側で実行済み)
	if (!ImGuizmo::IsUsing())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_1)) m_operation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_2)) m_operation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_3)) m_operation = ImGuizmo::SCALE;
	}

	CheckHotReload();

	//-------------------------------------------------------
	// エフェクトエディタ専用のコンテナウィンドウ
	//	メインビューポートの右外側に初期配置することで、
	//	マルチビューポート機能により起動時から「別ウィンドウ」として分離表示される
	//-------------------------------------------------------
	ImGuiViewport* mainViewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos(
		ImVec2(mainViewport->Pos.x + mainViewport->Size.x + 20.0f, mainViewport->Pos.y),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(900.0f, 700.0f), ImGuiCond_FirstUseEver);

	ImGui::Begin("Effect Editor Window");
	{
		ImGuiID effectDockId = ImGui::GetID("EffectDockSpace");

		// このIDのノードがまだ存在しない場合のみ、既定レイアウトを構築する
		if (ImGui::DockBuilderGetNode(effectDockId) == nullptr)
		{
			SetupEffectDockLayout(effectDockId, ImGui::GetContentRegionAvail());
		}

		ImGui::DockSpace(effectDockId, ImVec2(0, 0));
	}
	ImGui::End();

	DrawMainMenu();
	DrawHierarchy();
	DrawInspector();
	DrawAssetPicker();
	DrawGizmo();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// メニュー(セーブ/ロード)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectEditor::DrawMainMenu()
{
	ImGui::Begin("Effect Editor");

	ImGui::InputText("Path", m_filePathBuf, sizeof(m_filePathBuf));

	if (ImGui::Button("Save")) { Save(m_filePathBuf); }
	ImGui::SameLine();
	if (ImGui::Button("Load")) { Load(m_filePathBuf); }
	ImGui::SameLine();
	ImGui::Checkbox("Auto Reload", &m_autoReload);

	ImGui::Text("Effects : %d", (int)m_objects.size());

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 階層ウィンドウ
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectEditor::DrawHierarchy()
{
	ImGui::Begin("Effect Hierarchy");

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
// インスペクターウィンドウ
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectEditor::DrawInspector()
{
	ImGui::Begin("Effect Inspector");

	if (m_selected < 0 || m_selected >= (int)m_objects.size())
	{
		ImGui::TextDisabled("エフェクトが選択されていません");
		ImGui::End();
		return;
	}

	EffectObject& obj = m_objects[m_selected];

	char nameBuf[128];
	strcpy_s(nameBuf, obj.name.c_str());
	if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
	{
		obj.name = nameBuf;
	}

	ImGui::Text("Effect : %s", obj.effectPath.empty() ? "(None)" : obj.effectPath.c_str());

	bool transformChanged = false;
	transformChanged |= ImGui::DragFloat3("Position", &obj.pos.x, 0.1f);
	transformChanged |= ImGui::DragFloat3("Rotation", &obj.rotate.x, 1.0f);
	transformChanged |= ImGui::DragFloat3("Scale", &obj.scale.x, 0.05f, 0.01f, 100.0f);

	// 数値欄で編集した場合も、再生中ならその場でプレビューへ反映
	if (transformChanged)
	{
		if (auto sp = obj.wpPlaying.lock())
		{
			sp->SetWorldMatrix(obj.GetMatrix());
		}
	}

	if (ImGui::DragFloat("Speed", &obj.speed, 0.05f, 0.0f, 10.0f))
	{
		if (auto sp = obj.wpPlaying.lock()) sp->SetSpeed(obj.speed);
	}

	if (ImGui::Checkbox("Loop", &obj.loop))
	{
		if (auto sp = obj.wpPlaying.lock()) sp->SetLoop(obj.loop);
	}

	ImGui::Separator();

	bool playing = obj.IsPlaying();
	if (!playing)
	{
		if (ImGui::Button("Play")) PlayPreview(obj);
	}
	else
	{
		if (ImGui::Button("Stop")) StopPreview(obj);
	}

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
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectEditor::DrawGizmo()
{
	if (m_selected < 0 || m_selected >= (int)m_objects.size()) return;

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();

	const ImVec2& rectPos = EditorViewport::Instance().GetScreenPos();
	const ImVec2& rectSize = EditorViewport::Instance().GetScreenSize();
	ImGuizmo::SetRect(rectPos.x, rectPos.y, rectSize.x, rectSize.y);

	const auto& cameraCB = KdShaderManager::Instance().GetCameraCB();
	const DirectX::SimpleMath::Matrix& view = cameraCB.mView;
	const DirectX::SimpleMath::Matrix& proj = cameraCB.mProj;

	EffectObject& obj = m_objects[m_selected];

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

		// ギズモ操作中も再生中プレビューへライブ反映
		if (auto sp = obj.wpPlaying.lock())
		{
			sp->SetWorldMatrix(obj.GetMatrix());
		}
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アセット(エフェクトファイル)選択ウィンドウ
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectEditor::DrawAssetPicker()
{
	ImGui::Begin("Effect Assets");

	if (!m_assetListLoaded)
	{
		RefreshEffectFileList();
		m_assetListLoaded = true;
	}

	if (ImGui::Button("Refresh"))
	{
		RefreshEffectFileList();
	}

	ImGui::Separator();

	if (m_selected < 0 || m_selected >= (int)m_objects.size())
	{
		ImGui::TextDisabled("エフェクトオブジェクトを選択してください");
		ImGui::End();
		return;
	}

	EffectObject& obj = m_objects[m_selected];

	ImGui::Text("Current : %s", obj.effectPath.empty() ? "(None)" : obj.effectPath.c_str());
	ImGui::Separator();

	for (auto& path : m_effectFileList)
	{
		bool isSelected = (obj.effectPath == path);
		if (ImGui::Selectable(path.c_str(), isSelected))
		{
			// 差し替え前に現在のプレビューは一旦停止する
			StopPreview(obj);
			obj.effectPath = path;
		}
	}

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// EffekseerPath(KdEffekseerManager.h で定義) 以下を走査して .efk ファイル一覧を更新する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectEditor::RefreshEffectFileList()
{
	m_effectFileList.clear();

	namespace fs = std::filesystem;

	const std::string root = EffekseerPath;	// "Asset/Data/Effect/"

	if (!fs::exists(root)) return;

	for (auto& entry : fs::recursive_directory_iterator(root))
	{
		if (!entry.is_regular_file()) continue;
		if (entry.path().extension().string() != ".efk") continue;

		// KdEffekseerManager::Play() へ渡す形式(EffekseerPathからの相対パス)で保持する
		std::string relativePath = fs::relative(entry.path(), root).generic_string();
		m_effectFileList.push_back(relativePath);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// プレビュー再生の開始/停止
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectEditor::PlayPreview(EffectObject& obj)
{
	if (obj.effectPath.empty()) return;

	// 既に再生中なら一旦止めてから再生し直す
	StopPreview(obj);

	auto wp = KdEffekseerManager::GetInstance().Play(obj.effectPath, obj.pos, 1.0f, obj.speed, obj.loop);
	obj.wpPlaying = wp;

	if (auto sp = wp.lock())
	{
		// 位置に加えて回転・スケールも反映するため、改めてワールド行列で設定し直す
		sp->SetWorldMatrix(obj.GetMatrix());
		sp->SetSpeed(obj.speed);
		sp->SetLoop(obj.loop);
	}
}

void EffectEditor::StopPreview(EffectObject& obj)
{
	if (auto sp = obj.wpPlaying.lock())
	{
		KdEffekseerManager::GetInstance().StopEffect(sp->GetHandle());
	}
	obj.wpPlaying.reset();
}

void EffectEditor::PlayAllLooping()
{
	for (auto& obj : m_objects)
	{
		if (obj.loop && !obj.IsPlaying())
		{
			PlayPreview(obj);
		}
	}
}

void EffectEditor::StopAllPreview()
{
	for (auto& obj : m_objects)
	{
		StopPreview(obj);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// オブジェクトの追加/削除
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectEditor::AddObject()
{
	EffectObject obj;
	obj.name = "Effect" + std::to_string(m_objects.size());
	m_objects.push_back(obj);
	m_selected = (int)m_objects.size() - 1;
}

void EffectEditor::RemoveSelected()
{
	if (m_selected < 0 || m_selected >= (int)m_objects.size()) return;

	StopPreview(m_objects[m_selected]);
	m_objects.erase(m_objects.begin() + m_selected);
	m_selected = -1;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// セーブ/ロード
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectEditor::Save(const std::string& path)
{
	nlohmann::json j;

	for (auto& obj : m_objects)
	{
		j.push_back({
			{ "name",   obj.name },
			{ "effect", obj.effectPath },
			{ "pos",    { obj.pos.x, obj.pos.y, obj.pos.z } },
			{ "rotate", { obj.rotate.x, obj.rotate.y, obj.rotate.z } },
			{ "scale",  { obj.scale.x, obj.scale.y, obj.scale.z } },
			{ "speed",  obj.speed },
			{ "loop",   obj.loop }
			});
	}

	std::ofstream ofs(path);
	if (!ofs)
	{
		KdDebugGUI::Instance().AddLog("EffectEditor: 保存に失敗 %s\n", path.c_str());
		return;
	}

	ofs << j.dump(2);
	ofs.close();

	WIN32_FILE_ATTRIBUTE_DATA data;
	if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data))
	{
		m_lastWriteTime = data.ftLastWriteTime;
	}

	KdDebugGUI::Instance().AddLog("EffectEditor: 保存しました %s\n", path.c_str());
}

void EffectEditor::Load(const std::string& path)
{
	std::ifstream ifs(path);
	if (!ifs)
	{
		KdDebugGUI::Instance().AddLog("EffectEditor: 読み込み失敗 %s\n", path.c_str());
		return;
	}

	nlohmann::json j;
	ifs >> j;

	// 読み込み前に、現在再生中のプレビューを全て止めておく
	for (auto& obj : m_objects)
	{
		StopPreview(obj);
	}
	m_objects.clear();

	for (auto& e : j)
	{
		EffectObject obj;
		obj.name = e.at("name").get<std::string>();
		obj.effectPath = e.value("effect", std::string());
		obj.pos = { e.at("pos")[0],    e.at("pos")[1],    e.at("pos")[2] };
		obj.rotate = { e.at("rotate")[0], e.at("rotate")[1], e.at("rotate")[2] };
		obj.scale = { e.at("scale")[0],  e.at("scale")[1],  e.at("scale")[2] };
		obj.speed = e.value("speed", 1.0f);
		obj.loop = e.value("loop", false);

		m_objects.push_back(obj);
	}

	m_selected = -1;
	KdDebugGUI::Instance().AddLog("EffectEditor: 読み込みました %s\n", path.c_str());
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップデータ(JSON)の更新日時をポーリングし、外部から変更されていたら自動で再読み込みする
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectEditor::CheckHotReload()
{
	if (!m_autoReload) return;

	m_reloadCheckTimer += Application::Instance().GetDeltaTime();
	if (m_reloadCheckTimer < 0.5f) return;
	m_reloadCheckTimer = 0.0f;

	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!GetFileAttributesExA(m_filePathBuf, GetFileExInfoStandard, &data))
	{
		return;
	}

	if (m_lastWriteTime.dwLowDateTime == 0 && m_lastWriteTime.dwHighDateTime == 0)
	{
		m_lastWriteTime = data.ftLastWriteTime;
		return;
	}

	if (CompareFileTime(&data.ftLastWriteTime, &m_lastWriteTime) == 0)
	{
		return;
	}

	m_lastWriteTime = data.ftLastWriteTime;

	int keepSelected = m_selected;

	Load(m_filePathBuf);

	if (keepSelected >= 0 && keepSelected < (int)m_objects.size())
	{
		m_selected = keepSelected;
	}

	KdDebugGUI::Instance().AddLog("EffectEditor: 外部変更を検知し自動リロードしました (%s)\n", m_filePathBuf);
}