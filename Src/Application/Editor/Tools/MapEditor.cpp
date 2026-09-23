#include "Application/main.h"

#include "MapEditor.h"
#include "../Common/EditorViewport.h"
#include "Application/Factories/ComponentRegistry.h"

#include "Application/Core/EventBus/Events/SceneEvents.h"

#include "imgui_internal.h"

#include <filesystem>
#include <commdlg.h>	
#pragma comment(lib, "comdlg32.lib")
#include "nlohmann/json.hpp"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// data.pos/rotation(Quaternion)/scale から行列を作る。
// ImGuizmoのEuler経由(RecomposeMatrixFromComponents)は使わない。
// (フェーズ0で回転をQuaternion一本化した理由の一つが、この変換をやめて
//  合成順序の不一致による見た目のズレを構造的に無くすこと)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
DirectX::SimpleMath::Matrix MapObject::GetMatrix() const
{
	using namespace DirectX::SimpleMath;
	return Matrix::CreateScale(data.scale)
		* Matrix::CreateFromQuaternion(data.rotation)
		* Matrix::CreateTranslation(data.pos);
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

	ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.30f, nullptr, &center);
	ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.35f, nullptr, &center);
	ImGuiID preview = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.45f, nullptr, &center);

	ImGui::DockBuilderDockWindow("Hierarchy", left);
	ImGui::DockBuilderDockWindow("Inspector", center);
	ImGui::DockBuilderDockWindow("Map Preview", preview);
	ImGui::DockBuilderDockWindow("Assets", bottom);
	ImGui::DockBuilderDockWindow("Map Editor", bottom);

	ImGui::DockBuilderFinish(dockspaceId);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// m_objects内をIdで探す。生indexで触れる箇所をここに集約する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
MapObject* MapEditor::FindObject(ObjectId id)
{
	if (id == kInvalidObjectId) return nullptr;

	for (auto& obj : m_objects)
	{
		if (obj.data.id == id) return &obj;
	}
	return nullptr;
}

const MapObject* MapEditor::FindObject(ObjectId id) const
{
	if (id == kInvalidObjectId) return nullptr;

	for (auto& obj : m_objects)
	{
		if (obj.data.id == id) return &obj;
	}
	return nullptr;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Undo / Redo
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::PushUndo()
{
	UndoState state;
	state.objects = m_objects;	// MapObjectはKdModelWork(shared_ptr経由)を持つのでコピーは軽量
	state.selectedId = m_selectedId;

	m_undoStack.push_back(std::move(state));
	if (m_undoStack.size() > kMaxUndoDepth)
	{
		m_undoStack.erase(m_undoStack.begin());
	}

	m_redoStack.clear();
}

void MapEditor::Undo()
{
	if (m_undoStack.empty()) return;

	UndoState redoState;
	redoState.objects = m_objects;
	redoState.selectedId = m_selectedId;
	m_redoStack.push_back(std::move(redoState));

	UndoState prev = std::move(m_undoStack.back());
	m_undoStack.pop_back();

	m_objects = std::move(prev.objects);
	m_selectedId = prev.selectedId;	// 安定IDなので、Undo後もindexズレを気にせず正しいオブジェクトを指す
}

void MapEditor::Redo()
{
	if (m_redoStack.empty()) return;

	UndoState undoState;
	undoState.objects = m_objects;
	undoState.selectedId = m_selectedId;
	m_undoStack.push_back(std::move(undoState));

	UndoState next = std::move(m_redoStack.back());
	m_redoStack.pop_back();

	m_objects = std::move(next.objects);
	m_selectedId = next.selectedId;
}

void MapEditor::Update()
{
	ImGuizmo::BeginFrame();

	if (!ImGuizmo::IsUsing())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_1)) m_operation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_2)) m_operation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_3)) m_operation = ImGuizmo::SCALE;
	}

	if (!ImGui::GetIO().WantTextInput)
	{
		bool ctrl = ImGui::GetIO().KeyCtrl;
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
		{
			if (ImGui::GetIO().KeyShift) { Redo(); }
			else { Undo(); }
		}
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
		{
			Redo();
		}
	}

	CheckHotReload();

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

	ImGui::Separator();

	ImGui::BeginDisabled(m_undoStack.empty());
	if (ImGui::Button("Undo (Ctrl+Z)")) { Undo(); }
	ImGui::EndDisabled();

	ImGui::SameLine();

	ImGui::BeginDisabled(m_redoStack.empty());
	if (ImGui::Button("Redo (Ctrl+Y)")) { Redo(); }
	ImGui::EndDisabled();

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

	for (auto& obj : m_objects)
	{
		bool isSelected = (m_selectedId == obj.data.id);

		// ラベルの一意化はvector indexではなくIdで行う(indexは並び替え等で変わりうる為)
		std::string label = obj.data.name + "##" + std::to_string(obj.data.id);
		if (ImGui::Selectable(label.c_str(), isSelected))
		{
			m_selectedId = obj.data.id;
		}
	}

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// インスペクターウィンドウ(選択中オブジェクトのTransform + コンポーネント + ギズモ操作モード)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::DrawInspector()
{
	ImGui::Begin("Inspector");

	MapObject* selected = FindSelected();
	if (!selected)
	{
		ImGui::TextDisabled("オブジェクトが選択されていません");
		ImGui::End();
		return;
	}

	MapObject& obj = *selected;

	char nameBuf[128];
	strcpy_s(nameBuf, obj.data.name.c_str());
	ImGui::InputText("Name", nameBuf, sizeof(nameBuf));
	if (ImGui::IsItemActivated())
	{
		PushUndo();
	}
	if (ImGui::IsItemEdited())
	{
		obj.data.name = nameBuf;
	}

	ImGui::DragFloat3("Position", &obj.data.pos.x, 0.1f);
	if (ImGui::IsItemActivated()) { PushUndo(); }

	// 回転はdata.rotation(Quaternion)が正だが、Inspector上ではEuler角(度)で編集させる。
	// 選択が変わった時だけQuaternion→Eulerへ変換してキャッシュし、それ以外のフレームは
	// このキャッシュ値をそのまま表示・編集する(毎フレーム変換し直すとEuler表現の非一意性で
	// 値がガタつくことがある為)
	if (m_inspectorEulerForId != obj.data.id)
	{
		DirectX::SimpleMath::Vector3 eulerRad = obj.data.rotation.ToEuler();
		m_inspectorEulerDeg = {
			DirectX::XMConvertToDegrees(eulerRad.x),
			DirectX::XMConvertToDegrees(eulerRad.y),
			DirectX::XMConvertToDegrees(eulerRad.z)
		};
		m_inspectorEulerForId = obj.data.id;
	}

	ImGui::DragFloat3("Rotation", &m_inspectorEulerDeg.x, 1.0f);
	if (ImGui::IsItemActivated()) { PushUndo(); }
	if (ImGui::IsItemEdited())
	{
		obj.data.rotation = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(
			DirectX::XMConvertToRadians(m_inspectorEulerDeg.y),
			DirectX::XMConvertToRadians(m_inspectorEulerDeg.x),
			DirectX::XMConvertToRadians(m_inspectorEulerDeg.z));
	}

	ImGui::DragFloat3("Scale", &obj.data.scale.x, 0.1f, 0.01f, 100.0f);
	if (ImGui::IsItemActivated()) { PushUndo(); }

	ImGui::Separator();
	ComponentInspector::DrawList(
		obj.data.components,
		[this]() { PushUndo(); },
		[&obj](const std::string& type)
		{
			// ModelRenderが追加/削除/編集された時だけプレビュー用モデルを同期する
			if (type == "ModelRender") { obj.SyncPreviewModel(); }
		});

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
//	ImGuizmoのRecomposeMatrixFromComponents/DecomposeMatrixToComponents(Euler経由)は使わず、
//	SimpleMath::Matrix::CreateFromQuaternion/Decompose()で直接やり取りする。
//	これによりQuaternionの保存値を一切経由せず操作でき、Euler往復も発生しない
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::DrawGizmo()
{
	MapObject* selected = FindSelected();
	if (!selected) { m_gizmoWasUsing = false; return; }
	if (m_previewViewport.Width <= 0 || m_previewViewport.Height <= 0) { m_gizmoWasUsing = false; return; }

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();

	const ImVec2& rectPos = m_previewViewport.ScreenPos;
	const ImVec2& rectSize = m_previewViewport.ScreenSize;
	ImGuizmo::SetRect(rectPos.x, rectPos.y, rectSize.x, rectSize.y);

	DirectX::SimpleMath::Matrix view = m_previewCamera.GetView(GetPreviewTarget());
	DirectX::SimpleMath::Matrix proj = m_previewCamera.GetProj(
		(float)m_previewViewport.Width / (float)m_previewViewport.Height);

	MapObject& obj = *selected;

	DirectX::SimpleMath::Matrix matrix = obj.GetMatrix();

	ImGuizmo::Manipulate(
		reinterpret_cast<const float*>(&view),
		reinterpret_cast<const float*>(&proj),
		m_operation, m_mode, reinterpret_cast<float*>(&matrix),
		nullptr,
		m_useSnap ? m_snapValue : nullptr);

	bool isUsing = ImGuizmo::IsUsing();

	// ドラッグ「開始」の瞬間(前フレームは操作していなかった)だけUndoを1回積む。
	// この時点ではobj.dataはまだ書き換えていないので、正しく「操作前」のスナップショットになる
	if (isUsing && !m_gizmoWasUsing)
	{
		PushUndo();
	}

	if (isUsing)
	{
		DirectX::SimpleMath::Vector3 newScale, newPos;
		DirectX::SimpleMath::Quaternion newRot;
		matrix.Decompose(newScale, newRot, newPos);

		obj.data.pos = newPos;
		obj.data.rotation = newRot;
		obj.data.scale = newScale;

		// Inspector側のEuler表示キャッシュも追従させる(ギズモで回した直後にInspectorを
		// 見た時、古いEuler値のまま止まって見えないように)
		m_inspectorEulerForId = kInvalidObjectId;
	}

	m_gizmoWasUsing = isUsing;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アセット(モデル)選択ウィンドウ
//	Asset/Model 以下の .gltf/.glb を一覧表示し、選択中オブジェクトに割り当てる
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::DrawAssetPicker()
{
	ImGui::Begin("Assets");

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

			if (MapObject* target = FindSelected())
			{
				PushUndo();

				std::string fullPath = kAssetsFilePath + path;

				ComponentEntry* modelRender = nullptr;
				for (auto& c : target->data.components)
				{
					if (c.type == "ModelRender") { modelRender = &c; break; }
				}

				if (!modelRender)
				{
					ComponentEntry entry;
					entry.type = "ModelRender";
					if (const auto* defaults = ComponentRegistry::Instance().FindDefaultParams("ModelRender"))
					{
						entry.params = *defaults;
					}
					target->data.components.push_back(std::move(entry));
					modelRender = &target->data.components.back();
				}

				modelRender->params["model"] = fullPath;
				target->SyncPreviewModel();
			}
		}
	}

	ImGui::Separator();

	MapObject* selected = FindSelected();
	if (!selected)
	{
		ImGui::TextDisabled("オブジェクトを選択してください");
		ImGui::End();
		return;
	}

	std::string currentModel;
	for (auto& c : selected->data.components)
	{
		if (c.type == "ModelRender") { currentModel = c.params.value("model", std::string()); break; }
	}
	ImGui::Text("Current : %s", currentModel.empty() ? "(None)" : currentModel.c_str());

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
		return;
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
		relativePath = fileBuf;
	}

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
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::DrawObjects()
{
	for (auto& obj : m_objects)
	{
		if (!obj.modelWork.IsEnable()) continue;

		if (obj.modelWork.NeedCalcNodeMatrices())
		{
			obj.modelWork.CalcNodeMatrices();
		}

		KdShaderManager::Instance().m_StandardShader.DrawModel(obj.modelWork, obj.GetMatrix());
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// プレビュー用カメラの注視点：選択中オブジェクトがあればその位置。
// 未選択時は配置済みオブジェクト全体の重心(オブジェクトが無ければ原点)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
DirectX::SimpleMath::Vector3 MapEditor::GetPreviewTarget() const
{
	if (ImGuizmo::IsUsing())
	{
		return m_previewTargetCache;
	}

	DirectX::SimpleMath::Vector3 target = { 0,0,0 };

	if (const MapObject* selected = FindSelected())
	{
		target = selected->data.pos;
	}
	else if (!m_objects.empty())
	{
		DirectX::SimpleMath::Vector3 sum = { 0,0,0 };
		for (auto& obj : m_objects) { sum += obj.data.pos; }
		target = sum / (float)m_objects.size();
	}

	m_previewTargetCache = target;
	return target;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 配置済みオブジェクトの実描画
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::DrawPlacedObjects()
{
	DrawObjects();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップ全体を、専用カメラ・専用オフスクリーンバッファへ描画する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::RenderPreviewViewport()
{
	if (!m_previewViewport.Color || !m_previewViewport.Depth) return;
	if (m_previewViewport.Width <= 0 || m_previewViewport.Height <= 0) return;

	ID3D11DeviceContext* context = KdDirect3D::Instance().WorkDevContext();

	KdShaderManager::cbCamera savedCamera = KdShaderManager::Instance().GetCameraCB();

	ID3D11RenderTargetView* savedRTV = nullptr;
	ID3D11DepthStencilView* savedDSV = nullptr;
	context->OMGetRenderTargets(1, &savedRTV, &savedDSV);

	UINT savedVPNum = 1;
	D3D11_VIEWPORT savedVP = {};
	context->RSGetViewports(&savedVPNum, &savedVP);

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

	DirectX::SimpleMath::Vector3 target = GetPreviewTarget();

	DirectX::SimpleMath::Matrix view = m_previewCamera.GetView(target);
	DirectX::SimpleMath::Matrix proj = m_previewCamera.GetProj(
		(float)m_previewViewport.Width / (float)m_previewViewport.Height);

	KdShaderManager::Instance().WriteCBCamera(view.Invert(), proj);

	// ※WorkAmbientController().Draw()はここでは呼ばない。
	//   環境光・平行光のパラメータ自体は、このフレームの冒頭(Application::KdBeginDraw())で
	//   既にcb9へ書き込み済みなのでここで送り直す必要は無い。
	//   それどころか、Draw()内のWriteCBShadowArea()は無条件に呼ばれ、平行光の影生成エリアの
	//   中心位置を「その時点のカメラ位置(cb7のCamPos)」から計算するため、直前で書き換えた
	//   プレビュー用カメラの位置を基準に本編用のDirLight_mVPを上書きしてしまっていた
	//   (cbCamera自体は関数末尾で退避・復元しているが、このDirLight_mVPは対象外だった)。
	//   環境光・平行光の色などは既存の値をそのまま使えばよいため、このDraw()呼び出しごと削除する。

	KdShaderManager::Instance().m_StandardShader.BeginGenerateDepthMapFromLight();
	DrawObjects();
	KdShaderManager::Instance().m_StandardShader.EndGenerateDepthMapFromLight();
	KdShaderManager::Instance().m_StandardShader.BeginLit();
	DrawObjects();
	KdShaderManager::Instance().m_StandardShader.EndLit();


	KdShaderManager::Instance().WriteCBCamera(savedCamera.mView.Invert(), savedCamera.mProj);

	context->OMSetRenderTargets(1, &savedRTV, savedDSV);
	if (savedRTV) { savedRTV->Release(); }
	if (savedDSV) { savedDSV->Release(); }

	context->RSSetViewports(savedVPNum, &savedVP);

	// 通常描画パイプライン(このプレビュー分の描画)が完全に終わった後、カラーグレードを適用する。
	// ※Apply()内部は自前のRT/ビューポート退避・復元を行うため、ここでの追加の後始末は不要
	m_previewPostProcess.Apply(m_previewViewport.Color);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップ全体プレビューウィンドウ
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::DrawPreviewWindow()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("Map Preview", nullptr, ImGuiWindowFlags_NoMove);

	ImVec2 regionSize = ImGui::GetContentRegionAvail();

	if (regionSize.x >= 1.0f && regionSize.y >= 1.0f)
	{
		m_previewViewport.Resize((int)regionSize.x, (int)regionSize.y);
	}

	if (m_previewViewport.Color)
	{
		m_previewViewport.ScreenPos = ImGui::GetCursorScreenPos();
		m_previewViewport.ScreenSize = regionSize;

		// カラーグレード適用後の結果を表示する。リサイズ直後で結果がまだ無い場合のみ、
		// 生のプレビュー画像にフォールバックする(次フレームには結果が揃う)
		const std::shared_ptr<KdTexture>& displayTex =
			m_previewPostProcess.GetResultTexture() ? m_previewPostProcess.GetResultTexture() : m_previewViewport.Color;

		ImGui::Image((ImTextureID)displayTex->WorkSRView(), regionSize);

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
	if (w == Width && h == Height && Color && Depth) return;

	Width = w;
	Height = h;

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
	PushUndo();

	MapObject obj;
	obj.data.id = m_nextId++;
	obj.data.name = "Object" + std::to_string(obj.data.id);
	m_objects.push_back(obj);
	m_selectedId = obj.data.id;
}

void MapEditor::RemoveSelected()
{
	MapObject* selected = FindSelected();
	if (!selected) return;

	PushUndo();

	m_objects.erase(
		std::remove_if(m_objects.begin(), m_objects.end(),
			[this](const MapObject& o) { return o.data.id == m_selectedId; }),
		m_objects.end());

	m_selectedId = kInvalidObjectId;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// セーブ/ロード
//	実際のファイルI/O・スキーマ変換はMapData.h/.cppのLoadMapFile/SaveMapFileに一元化されている
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::Save(const std::string& path)
{
	std::vector<MapEntity> entities;
	entities.reserve(m_objects.size());
	for (auto& obj : m_objects) { entities.push_back(obj.data); }

	if (!SaveMapFile(path, entities))
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
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void MapEditor::CheckHotReload()
{
	if (!m_autoReload) return;

	m_reloadCheckTimer += Application::Instance().GetDeltaTime();
	if (m_reloadCheckTimer < 0.5f) return;
	m_reloadCheckTimer = 0.0f;

	FILETIME writeTime;
	if (!JsonLoader::GetLastWriteTime(m_filePathBuf, writeTime))
	{
		return;
	}

	if (m_lastWriteTime.dwLowDateTime == 0 && m_lastWriteTime.dwHighDateTime == 0)
	{
		m_lastWriteTime = writeTime;
		return;
	}

	if (CompareFileTime(&writeTime, &m_lastWriteTime) == 0)
	{
		return;
	}

	m_lastWriteTime = writeTime;

	// 選択状態は安定IDで保持しているので、Load()後もm_selectedIdをそのまま使い回せる
	// (同じIDのオブジェクトが引き続き存在すれば選択状態は自然に復元される。
	//  以前はvector indexで持っていたため、ここで明示的に退避/復元する必要があった)
	Load(m_filePathBuf);

	KdDebugGUI::Instance().AddLog("MapEditor: 外部変更を検知し自動リロードしました (%s)\n", m_filePathBuf);
}

void MapEditor::Load(const std::string& path)
{
	MapFile mapFile;
	if (!LoadMapFile(path, mapFile))
	{
		KdDebugGUI::Instance().AddLog("MapEditor: 読み込み失敗 %s\n", path.c_str());
		return;
	}

	std::vector<MapObject> loaded;
	loaded.reserve(mapFile.entities.size());

	for (auto& entity : mapFile.entities)
	{
		MapObject obj;
		obj.data = std::move(entity);
		obj.SyncPreviewModel();	// ここで実際のモデル読み込みが走る
		loaded.push_back(std::move(obj));
	}

	ObjectId keepSelectedId = m_selectedId;

	m_objects = std::move(loaded);
	m_nextId = mapFile.nextId;

	// 同じIDのオブジェクトがまだ存在すれば選択状態を維持する(無ければ自然に非選択になる)
	m_selectedId = FindObject(keepSelectedId) ? keepSelectedId : kInvalidObjectId;

	m_undoStack.clear();
	m_redoStack.clear();

	// 現在のシーンを再生成させる
	//GLOBALEVENT.Publish(Events::Scene::ReloadingSceneEvent());

	KdDebugGUI::Instance().AddLog("MapEditor: 読み込みました %s\n", path.c_str());
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// コンストラクタ：起動時に既存のマップデータ(m_filePathBuf)を自動ロードする
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
MapEditor::MapEditor()
{
	Load(m_filePathBuf);
	LoadModelRegistry(m_registryPathBuf);
}