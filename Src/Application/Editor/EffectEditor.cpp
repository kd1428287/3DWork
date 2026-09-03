//#include "../main.h"
//
//#include "EffectEditor.h"
//#include "EditorViewport.h"
//#include "../Effect/EffectDataLoader.h"
//
//#include "imgui_internal.h"
//
//#include <filesystem>
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// テクスチャアセットの走査ルート(実際のプロジェクト構成に合わせて調整)
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//static const std::string kTextureAssetRoot = "Asset/Texture/";
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// pos/rotate(degree)/scale から行列を作る(MapObjectと同じ仕組み)
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//DirectX::SimpleMath::Matrix EffectObject::GetMatrix() const
//{
//	float m[16];
//	ImGuizmo::RecomposeMatrixFromComponents(&pos.x, &rotate.x, &scale.x, m);
//
//	DirectX::SimpleMath::Matrix mat;
//	memcpy(&mat, m, sizeof(float) * 16);
//	return mat;
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// EffectEditor専用ドックスペースの初期レイアウト
////	左：Effect Hierarchy / 中央：Effect Inspector / 下：Effect Editor(メニュー) + Effect Assets(タブ)
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//static void SetupEffectDockLayout(ImGuiID dockspaceId, const ImVec2& size)
//{
//	ImGui::DockBuilderRemoveNode(dockspaceId);
//	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
//	ImGui::DockBuilderSetNodeSize(dockspaceId, size);
//
//	ImGuiID center = dockspaceId;
//
//	// 左：Effect Hierarchy(幅30%)
//	ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.30f, nullptr, &center);
//
//	// 下：Effect Editor(メニュー) + Effect Assets(タブ)、高さ35%
//	ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.35f, nullptr, &center);
//
//	ImGui::DockBuilderDockWindow("Effect Hierarchy", left);
//	ImGui::DockBuilderDockWindow("Effect Inspector", center);	// 残った中央上
//	ImGui::DockBuilderDockWindow("Effect Assets", bottom);
//	ImGui::DockBuilderDockWindow("Effect Editor", bottom);	// Effect Assetsとタブ化
//
//	ImGui::DockBuilderFinish(dockspaceId);
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// 毎フレーム更新
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::Update()
//{
//	// ショートカットキー(ImGuizmo::BeginFrame()はMapEditor::Update()側で実行済み)
//	if (!ImGuizmo::IsUsing())
//	{
//		if (ImGui::IsKeyPressed(ImGuiKey_1)) m_operation = ImGuizmo::TRANSLATE;
//		if (ImGui::IsKeyPressed(ImGuiKey_2)) m_operation = ImGuizmo::ROTATE;
//		if (ImGui::IsKeyPressed(ImGuiKey_3)) m_operation = ImGuizmo::SCALE;
//	}
//
//	CheckHotReload();
//
//	const float deltaTime = Application::Instance().GetDeltaTime();
//
//	// 再生中プレビューのシミュレーションを進める(発生のInterval/Rate管理含む)
//	// ※実際の描画はDrawPreviewParticles()で3D描画パス側から行う
//	for (auto& obj : m_objects)
//	{
//		if (obj.playing && !obj.paused)
//		{
//			UpdatePreview(obj, deltaTime);
//		}
//	}
//
//	// 鍔迫り合いの火花のテストプレビューも同様に更新する(発生自体はTestEmitWeaponClash()から)
//	if (m_weaponClashPreviewParticle)
//	{
//		m_weaponClashPreviewParticle->Update(deltaTime);
//	}
//
//	//-------------------------------------------------------
//	// エフェクトエディタ専用のコンテナウィンドウ
//	//	メインビューポートの右外側に初期配置することで、
//	//	マルチビューポート機能により起動時から「別ウィンドウ」として分離表示される
//	//-------------------------------------------------------
//	ImGuiViewport* mainViewport = ImGui::GetMainViewport();
//
//	ImGui::SetNextWindowPos(
//		ImVec2(mainViewport->Pos.x + mainViewport->Size.x + 20.0f, mainViewport->Pos.y),
//		ImGuiCond_FirstUseEver);
//	ImGui::SetNextWindowSize(ImVec2(900.0f, 700.0f), ImGuiCond_FirstUseEver);
//
//	ImGui::Begin("Effect Editor Window");
//	{
//		ImGuiID effectDockId = ImGui::GetID("EffectDockSpace");
//
//		// このIDのノードがまだ存在しない場合のみ、既定レイアウトを構築する
//		if (ImGui::DockBuilderGetNode(effectDockId) == nullptr)
//		{
//			SetupEffectDockLayout(effectDockId, ImGui::GetContentRegionAvail());
//		}
//
//		ImGui::DockSpace(effectDockId, ImVec2(0, 0));
//	}
//	ImGui::End();
//
//	DrawMainMenu();
//	DrawHierarchy();
//	DrawInspector();
//	DrawTexturePicker();
//	DrawGizmo();
//
//	// Weapon Clashは配置を持たないグローバル設定のため、専用のフロートウィンドウとして表示する
//	DrawWeaponClashInspector();
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// プレビュー中のGPUパーティクルを描画する(3D描画パスの半透明タイミングから呼ぶ)
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::DrawPreviewParticles()
//{
//	for (auto& obj : m_objects)
//	{
//		if (obj.playing && obj.previewParticle)
//		{
//			obj.previewParticle->Draw(obj.previewTexture);
//		}
//	}
//
//	if (m_weaponClashPreviewParticle)
//	{
//		m_weaponClashPreviewParticle->Draw(nullptr);
//	}
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// メニュー(セーブ/ロード)
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::DrawMainMenu()
//{
//	ImGui::Begin("Effect Editor");
//
//	ImGui::InputText("Path", m_filePathBuf, sizeof(m_filePathBuf));
//
//	if (ImGui::Button("Save")) { Save(m_filePathBuf); }
//	ImGui::SameLine();
//	if (ImGui::Button("Load")) { Load(m_filePathBuf); }
//	ImGui::SameLine();
//	ImGui::Checkbox("Auto Reload", &m_autoReload);
//
//	ImGui::Separator();
//
//	if (ImGui::Button("Play All")) { PlayAllPreview(); }
//	ImGui::SameLine();
//	if (ImGui::Button("Stop All")) { StopAllPreview(); }
//
//	ImGui::Text("Effects : %d", (int)m_objects.size());
//
//	ImGui::End();
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// 階層ウィンドウ
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::DrawHierarchy()
//{
//	ImGui::Begin("Effect Hierarchy");
//
//	if (ImGui::Button("+ Add"))
//	{
//		AddObject();
//	}
//	ImGui::SameLine();
//	if (ImGui::Button("- Remove"))
//	{
//		RemoveSelected();
//	}
//
//	ImGui::Separator();
//
//	for (int i = 0; i < (int)m_objects.size(); i++)
//	{
//		bool isSelected = (m_selected == i);
//
//		std::string label = m_objects[i].name + "##" + std::to_string(i);
//		if (ImGui::Selectable(label.c_str(), isSelected))
//		{
//			m_selected = i;
//		}
//	}
//
//	ImGui::End();
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// インスペクターウィンドウ
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::DrawInspector()
//{
//	ImGui::Begin("Effect Inspector");
//
//	if (m_selected < 0 || m_selected >= (int)m_objects.size())
//	{
//		ImGui::TextDisabled("エフェクトが選択されていません");
//		ImGui::End();
//		return;
//	}
//
//	EffectObject& obj = m_objects[m_selected];
//	GPUParticleParams& params = obj.params;
//
//	char nameBuf[128];
//	strcpy_s(nameBuf, obj.name.c_str());
//	if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
//	{
//		obj.name = nameBuf;
//	}
//
//	ImGui::DragFloat3("Position", &obj.pos.x, 0.1f);
//	ImGui::DragFloat3("Rotation", &obj.rotate.x, 1.0f);
//	ImGui::DragFloat3("Scale", &obj.scale.x, 0.05f, 0.01f, 100.0f);
//
//	ImGui::Separator();
//	ImGui::Text("Emission");
//
//	{
//		int capacity = (int)params.MaxParticleNum;
//		if (ImGui::DragInt("Max Particle Num", &capacity, 1.0f, 1, 100000))
//		{
//			params.MaxParticleNum = (UINT)std::max(1, capacity);
//		}
//	}
//
//	{
//		const char* modeLabels[] = { "Burst", "Continuous" };
//		int modeIdx = (params.EmitMode == KdParticleEmitMode::Continuous) ? 1 : 0;
//		if (ImGui::Combo("Emit Mode", &modeIdx, modeLabels, IM_ARRAYSIZE(modeLabels)))
//		{
//			params.EmitMode = (modeIdx == 1) ? KdParticleEmitMode::Continuous : KdParticleEmitMode::Burst;
//		}
//	}
//
//	if (params.EmitMode == KdParticleEmitMode::Burst)
//	{
//		ImGui::DragInt("Emit Count", &params.EmitCount, 1.0f, 1, 100000);
//		ImGui::DragFloat("Emit Interval(sec)", &params.EmitInterval, 0.05f, 0.0f, 60.0f);
//		ImGui::TextDisabled("Interval<=0 : 再生開始時に1回だけ発生");
//	}
//	else
//	{
//		ImGui::DragFloat("Emit Rate(/sec)", &params.EmitRate, 1.0f, 0.0f, 100000.0f);
//	}
//
//	ImGui::Separator();
//	ImGui::Text("Particle");
//
//	ImGui::DragFloat3("Velocity Min", &params.VelocityMin.x, 0.05f);
//	ImGui::DragFloat3("Velocity Max", &params.VelocityMax.x, 0.05f);
//	ImGui::DragFloatRange2("Size Min/Max", &params.SizeMin, &params.SizeMax, 0.01f, 0.001f, 100.0f);
//	ImGui::DragFloatRange2("Life Min/Max(sec)", &params.LifeMin, &params.LifeMax, 0.02f, 0.01f, 60.0f);
//	ImGui::ColorEdit4("Color", &params.Color.x);
//	ImGui::DragFloat3("Gravity", &params.Gravity.x, 0.05f);
//
//	ImGui::Separator();
//	ImGui::Text("Material (WIP)");
//	ImGui::Text("Texture : %s", params.TexturePath.empty() ? "(None)" : params.TexturePath.c_str());
//
//	{
//		const char* blendLabels[] = { "Add", "Alpha" };
//		int blendIdx = (params.BlendMode == KdParticleBlendMode::Alpha) ? 1 : 0;
//		if (ImGui::Combo("Blend Mode", &blendIdx, blendLabels, IM_ARRAYSIZE(blendLabels)))
//		{
//			params.BlendMode = (blendIdx == 1) ? KdParticleBlendMode::Alpha : KdParticleBlendMode::Add;
//		}
//		ImGui::TextDisabled("※現状KdGPUParticle::Drawは加算合成固定。実際に切り替えるにはDraw側の対応が必要");
//	}
//
//	ImGui::Separator();
//
//	bool playing = obj.IsPlaying();
//	if (!playing)
//	{
//		if (ImGui::Button("Play")) PlayPreview(obj);
//	}
//	else
//	{
//		if (ImGui::Button("Stop")) StopPreview(obj);
//		ImGui::SameLine();
//
//		if (!obj.paused)
//		{
//			if (ImGui::Button("Pause")) SetPreviewPause(obj, true);
//		}
//		else
//		{
//			if (ImGui::Button("Resume")) SetPreviewPause(obj, false);
//		}
//		ImGui::SameLine();
//
//		if (ImGui::Button("Restart")) PlayPreview(obj);
//	}
//
//	ImGui::Separator();
//	ImGui::Text("Gizmo Operation");
//
//	if (ImGui::RadioButton("Translate(1)", m_operation == ImGuizmo::TRANSLATE)) m_operation = ImGuizmo::TRANSLATE;
//	ImGui::SameLine();
//	if (ImGui::RadioButton("Rotate(2)", m_operation == ImGuizmo::ROTATE)) m_operation = ImGuizmo::ROTATE;
//	ImGui::SameLine();
//	if (ImGui::RadioButton("Scale(3)", m_operation == ImGuizmo::SCALE)) m_operation = ImGuizmo::SCALE;
//
//	if (m_operation != ImGuizmo::SCALE)
//	{
//		if (ImGui::RadioButton("World", m_mode == ImGuizmo::WORLD)) m_mode = ImGuizmo::WORLD;
//		ImGui::SameLine();
//		if (ImGui::RadioButton("Local", m_mode == ImGuizmo::LOCAL)) m_mode = ImGuizmo::LOCAL;
//	}
//
//	ImGui::Checkbox("Snap", &m_useSnap);
//	if (m_useSnap)
//	{
//		if (m_operation == ImGuizmo::TRANSLATE)
//			ImGui::DragFloat3("SnapValue", m_snapValue, 0.1f);
//		else
//			ImGui::DragFloat("SnapValue", m_snapValue, 0.5f);
//	}
//
//	ImGui::End();
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// ギズモ描画・操作
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::DrawGizmo()
//{
//	if (m_selected < 0 || m_selected >= (int)m_objects.size()) return;
//
//	ImGuizmo::SetOrthographic(false);
//	ImGuizmo::SetDrawlist();
//
//	const ImVec2& rectPos = EditorViewport::Instance().GetScreenPos();
//	const ImVec2& rectSize = EditorViewport::Instance().GetScreenSize();
//	ImGuizmo::SetRect(rectPos.x, rectPos.y, rectSize.x, rectSize.y);
//
//	const auto& cameraCB = KdShaderManager::Instance().GetCameraCB();
//	const DirectX::SimpleMath::Matrix& view = cameraCB.mView;
//	const DirectX::SimpleMath::Matrix& proj = cameraCB.mProj;
//
//	EffectObject& obj = m_objects[m_selected];
//
//	float matrix[16];
//	ImGuizmo::RecomposeMatrixFromComponents(&obj.pos.x, &obj.rotate.x, &obj.scale.x, matrix);
//
//	ImGuizmo::Manipulate(
//		reinterpret_cast<const float*>(&view),
//		reinterpret_cast<const float*>(&proj),
//		m_operation, m_mode, matrix,
//		nullptr,
//		m_useSnap ? m_snapValue : nullptr);
//
//	if (ImGuizmo::IsUsing())
//	{
//		ImGuizmo::DecomposeMatrixToComponents(matrix, &obj.pos.x, &obj.rotate.x, &obj.scale.x);
//	}
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// アセット(テクスチャファイル)選択ウィンドウ
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::DrawTexturePicker()
//{
//	ImGui::Begin("Effect Assets");
//
//	if (!m_textureListLoaded)
//	{
//		RefreshTextureFileList();
//		m_textureListLoaded = true;
//	}
//
//	if (ImGui::Button("Refresh"))
//	{
//		RefreshTextureFileList();
//	}
//	ImGui::SameLine();
//	ImGui::Checkbox("Auto Preview", &m_autoPreviewOnSelect);
//
//	ImGui::Separator();
//
//	if (m_selected < 0 || m_selected >= (int)m_objects.size())
//	{
//		ImGui::TextDisabled("エフェクトオブジェクトを選択してください");
//		ImGui::End();
//		return;
//	}
//
//	EffectObject& obj = m_objects[m_selected];
//
//	ImGui::Text("Current : %s", obj.params.TexturePath.empty() ? "(None)" : obj.params.TexturePath.c_str());
//	ImGui::Separator();
//
//	for (auto& path : m_textureFileList)
//	{
//		bool isSelected = (obj.params.TexturePath == path);
//		if (ImGui::Selectable(path.c_str(), isSelected))
//		{
//			obj.params.TexturePath = path;
//
//			// テクスチャキャッシュを次回Play/Update時に読み直させる
//			obj.previewTexture.reset();
//
//			// Auto Preview有効時は選択した瞬間にその場で再生し、見た目をすぐ確認できるようにする
//			if (m_autoPreviewOnSelect)
//			{
//				PlayPreview(obj);
//			}
//			else if (obj.playing)
//			{
//				// 再生中に差し替えた場合はその場でテクスチャだけ読み直す
//				EnsurePreviewTexture(obj);
//			}
//		}
//	}
//
//	ImGui::End();
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// 鍔迫り合いの火花(WeaponClashEffectEvent)専用インスペクター
////	配置を持たない単一のグローバル設定のため、Effect Hierarchy/Inspectorとは独立した
////	フロートウィンドウとして表示する。Main/Emberそれぞれの層をCollapsingHeaderで編集し、
////	固定方向(+X)でのテスト再生ができる
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::DrawWeaponClashInspector()
//{
//	ImGui::SetNextWindowSize(ImVec2(420.0f, 560.0f), ImGuiCond_FirstUseEver);
//	ImGui::Begin("Weapon Clash");
//
//	WeaponClashEffectParams& p = m_weaponClash;
//
//	{
//		int capacity = (int)p.MaxParticleNum;
//		if (ImGui::DragInt("Max Particle Num", &capacity, 1.0f, 1, 100000))
//		{
//			p.MaxParticleNum = (UINT)std::max(1, capacity);
//		}
//	}
//
//	auto drawLayer = [](const char* label, DirectionalSparkLayer& layer)
//		{
//			if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) { return; }
//
//			ImGui::PushID(label);
//
//			ImGui::DragFloat("Dir Scale Min", &layer.DirScaleMin, 0.05f);
//			ImGui::DragFloat("Dir Scale Max", &layer.DirScaleMax, 0.05f);
//			ImGui::DragFloat3("Offset Min", &layer.OffsetMin.x, 0.05f);
//			ImGui::DragFloat3("Offset Max", &layer.OffsetMax.x, 0.05f);
//			ImGui::DragFloatRange2("Size Min/Max", &layer.SizeMin, &layer.SizeMax, 0.01f, 0.001f, 100.0f);
//			ImGui::DragFloatRange2("Life Min/Max(sec)", &layer.LifeMin, &layer.LifeMax, 0.02f, 0.01f, 60.0f);
//			ImGui::ColorEdit4("Color", &layer.Color.x);
//
//			{
//				int countParry = (int)layer.CountParry;
//				if (ImGui::DragInt("Count(Parry)", &countParry, 1.0f, 0, 100000))
//				{
//					layer.CountParry = (UINT)std::max(0, countParry);
//				}
//			}
//			{
//				int countBlock = (int)layer.CountBlock;
//				if (ImGui::DragInt("Count(Block)", &countBlock, 1.0f, 0, 100000))
//				{
//					layer.CountBlock = (UINT)std::max(0, countBlock);
//				}
//			}
//
//			ImGui::PopID();
//		};
//
//	drawLayer("Main (勢いよく飛ぶ火花)", p.Main);
//	drawLayer("Ember (ゆっくり落ちるくすぶり)", p.Ember);
//
//	ImGui::Separator();
//	ImGui::TextDisabled("プレビューは固定方向(+X)で再生します(実際の発生方向は武器の進行方向で決まる)");
//
//	if (ImGui::Button("Test Parry")) { TestEmitWeaponClash(true); }
//	ImGui::SameLine();
//	if (ImGui::Button("Test Block")) { TestEmitWeaponClash(false); }
//
//	ImGui::End();
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// テクスチャアセットディレクトリ以下を走査して画像ファイル一覧を更新する
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::RefreshTextureFileList()
//{
//	m_textureFileList.clear();
//
//	namespace fs = std::filesystem;
//
//	const std::string root = kTextureAssetRoot;
//
//	if (!fs::exists(root)) return;
//
//	static const std::vector<std::string> kExtensions = { ".png", ".dds", ".jpg", ".jpeg", ".tga" };
//
//	for (auto& entry : fs::recursive_directory_iterator(root))
//	{
//		if (!entry.is_regular_file()) continue;
//
//		std::string ext = entry.path().extension().string();
//		for (auto& c : ext) c = (char)tolower(c);
//
//		if (std::find(kExtensions.begin(), kExtensions.end(), ext) == kExtensions.end()) continue;
//
//		// KdResourceFactory::GetTexture() へ渡す形式(ルートからの相対パス)で保持する
//		std::string relativePath = fs::relative(entry.path(), root).generic_string();
//		m_textureFileList.push_back(relativePath);
//	}
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// params.MaxParticleNumに合わせてpreviewParticleを(必要なら)作り直す
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::EnsurePreviewParticleCapacity(EffectObject& obj)
//{
//	if (obj.previewParticle && obj.previewCapacity == obj.params.MaxParticleNum) { return; }
//
//	obj.previewParticle = std::make_shared<KdGPUParticle>();
//	obj.previewParticle->Init(obj.params.MaxParticleNum);
//	obj.previewCapacity = obj.params.MaxParticleNum;
//
//	// 発生カウンタ等はInit()でリセットされる為、経過状態もリセットしておく
//	obj.emitAccumulator = 0.0f;
//	obj.burstTimer = 0.0f;
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// params.TexturePathに合わせてpreviewTextureを(必要なら)読み込み直す
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::EnsurePreviewTexture(EffectObject& obj)
//{
//	if (obj.params.TexturePath.empty())
//	{
//		obj.previewTexture.reset();
//		return;
//	}
//
//	if (obj.previewTexture) { return; }
//
//	obj.previewTexture = KdAssets::Instance().m_textures.GetData(kTextureAssetRoot + obj.params.TexturePath);
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// m_weaponClash.MaxParticleNumに合わせてm_weaponClashPreviewParticleを(必要なら)作り直す
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::EnsureWeaponClashPreviewCapacity()
//{
//	if (m_weaponClashPreviewParticle && m_weaponClashPreviewCapacity == m_weaponClash.MaxParticleNum) { return; }
//
//	m_weaponClashPreviewParticle = std::make_shared<KdGPUParticle>();
//	m_weaponClashPreviewParticle->Init(m_weaponClash.MaxParticleNum);
//	m_weaponClashPreviewCapacity = m_weaponClash.MaxParticleNum;
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// 鍔迫り合いの火花を固定方向(+X)、原点でテスト再生する
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::TestEmitWeaponClash(bool isParry)
//{
//	EnsureWeaponClashPreviewCapacity();
//
//	// プレビュー用の固定条件(実際の発生位置/方向はWeaponClashEffectEvent::Position、
//	// SelfWeaponDir - OtherWeaponDirで決まる)
//	static const DirectX::SimpleMath::Vector3 kPreviewPos = { 0.0f, 0.0f, 0.0f };
//	static const DirectX::SimpleMath::Vector3 kPreviewBaseDir = { 1.0f, 0.0f, 0.0f };
//
//	m_weaponClashPreviewParticle->Emit(
//		m_weaponClash.Main.ToEmitParameter(kPreviewPos, kPreviewBaseDir),
//		m_weaponClash.Main.GetEmitCount(isParry));
//
//	m_weaponClashPreviewParticle->Emit(
//		m_weaponClash.Ember.ToEmitParameter(kPreviewPos, kPreviewBaseDir),
//		m_weaponClash.Ember.GetEmitCount(isParry));
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// プレビュー再生の開始/停止/一時停止
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::PlayPreview(EffectObject& obj)
//{
//	EnsurePreviewParticleCapacity(obj);
//	EnsurePreviewTexture(obj);
//
//	obj.playing = true;
//	obj.paused = false;
//	obj.emitAccumulator = 0.0f;
//	obj.burstTimer = 0.0f;
//
//	// Burstモードは再生開始時にまず1回発生させる
//	if (obj.params.EmitMode == KdParticleEmitMode::Burst)
//	{
//		obj.previewParticle->Emit(obj.params.ToEmitParameter(obj.pos), (UINT)std::max(0, obj.params.EmitCount));
//		obj.burstTimer = obj.params.EmitInterval;
//	}
//}
//
//void EffectEditor::StopPreview(EffectObject& obj)
//{
//	obj.playing = false;
//	obj.paused = false;
//	obj.emitAccumulator = 0.0f;
//	obj.burstTimer = 0.0f;
//	// previewParticle/previewTextureはキャッシュとして保持したまま(Init/読み込みのやり直しコストを避ける為)
//}
//
//void EffectEditor::SetPreviewPause(EffectObject& obj, bool pause)
//{
//	if (!obj.playing) return;
//	obj.paused = pause;
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// 再生中プレビューの発生・シミュレーションを1フレームぶん進める
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::UpdatePreview(EffectObject& obj, float deltaTime)
//{
//	if (!obj.previewParticle) return;
//
//	GPUParticleParams& params = obj.params;
//
//	if (params.EmitMode == KdParticleEmitMode::Continuous)
//	{
//		obj.emitAccumulator += params.EmitRate * deltaTime;
//
//		UINT emitCount = (UINT)obj.emitAccumulator;
//		if (emitCount > 0)
//		{
//			obj.previewParticle->Emit(params.ToEmitParameter(obj.pos), emitCount);
//			obj.emitAccumulator -= (float)emitCount;
//		}
//	}
//	else // Burst
//	{
//		// Interval<=0の場合は再生開始時の1回のみ(自動リピートしない)
//		if (params.EmitInterval > 0.0f)
//		{
//			obj.burstTimer -= deltaTime;
//			if (obj.burstTimer <= 0.0f)
//			{
//				obj.previewParticle->Emit(params.ToEmitParameter(obj.pos), (UINT)std::max(0, params.EmitCount));
//				obj.burstTimer += params.EmitInterval;
//			}
//		}
//	}
//
//	obj.previewParticle->Update(deltaTime, params.Gravity);
//}
//
//void EffectEditor::PlayAllLooping()
//{
//	for (auto& obj : m_objects)
//	{
//		if (obj.params.IsLooping() && !obj.IsPlaying())
//		{
//			PlayPreview(obj);
//		}
//	}
//}
//
//void EffectEditor::PlayAllPreview()
//{
//	for (auto& obj : m_objects)
//	{
//		PlayPreview(obj);
//	}
//}
//
//void EffectEditor::StopAllPreview()
//{
//	for (auto& obj : m_objects)
//	{
//		StopPreview(obj);
//	}
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// オブジェクトの追加/削除
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::AddObject()
//{
//	EffectObject obj;
//	obj.name = "Effect" + std::to_string(m_objects.size());
//	m_objects.push_back(obj);
//	m_selected = (int)m_objects.size() - 1;
//}
//
//void EffectEditor::RemoveSelected()
//{
//	if (m_selected < 0 || m_selected >= (int)m_objects.size()) return;
//
//	StopPreview(m_objects[m_selected]);
//	m_objects.erase(m_objects.begin() + m_selected);
//	m_selected = -1;
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// セーブ/ロード
////	実際のファイルI/OとJSONスキーマはEffectDataLoaderに委譲する
////	(EffectDispatcherが読み込むスキーマと共通化するため)
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::Save(const std::string& path)
//{
//	EffectDataFile data;
//	data.Effects.reserve(m_objects.size());
//
//	for (auto& obj : m_objects)
//	{
//		EffectDefinition def;
//		def.Name = obj.name;
//		def.Pos = obj.pos;
//		def.Rotate = obj.rotate;
//		def.Scale = obj.scale;
//		def.Params = obj.params;
//		data.Effects.push_back(def);
//	}
//
//	data.WeaponClash = m_weaponClash;
//
//	if (!EffectDataLoader::Save(path, data))
//	{
//		KdDebugGUI::Instance().AddLog("EffectEditor: 保存に失敗 %s\n", path.c_str());
//		return;
//	}
//
//	FILETIME writeTime;
//	if (JsonLoader::GetLastWriteTime(path, writeTime))
//	{
//		m_lastWriteTime = writeTime;
//	}
//
//	KdDebugGUI::Instance().AddLog("EffectEditor: 保存しました %s\n", path.c_str());
//}
//
//void EffectEditor::Load(const std::string& path)
//{
//	EffectDataFile data;
//	if (!EffectDataLoader::Load(path, data))
//	{
//		KdDebugGUI::Instance().AddLog("EffectEditor: 読み込み失敗 %s\n", path.c_str());
//		return;
//	}
//
//	// 読み込み前に、現在再生中のプレビューを全て止めておく
//	for (auto& obj : m_objects)
//	{
//		StopPreview(obj);
//	}
//	m_objects.clear();
//
//	for (auto& def : data.Effects)
//	{
//		EffectObject obj;
//		obj.name = def.Name;
//		obj.pos = def.Pos;
//		obj.rotate = def.Rotate;
//		obj.scale = def.Scale;
//		obj.params = def.Params;
//		m_objects.push_back(obj);
//	}
//
//	m_weaponClash = data.WeaponClash;
//
//	m_selected = -1;
//	KdDebugGUI::Instance().AddLog("EffectEditor: 読み込みました %s\n", path.c_str());
//}
//
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//// マップデータ(JSON)の更新日時をポーリングし、外部から変更されていたら自動で再読み込みする
//// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//void EffectEditor::CheckHotReload()
//{
//	if (!m_autoReload) return;
//
//	m_reloadCheckTimer += Application::Instance().GetDeltaTime();
//	if (m_reloadCheckTimer < 0.5f) return;
//	m_reloadCheckTimer = 0.0f;
//
//	FILETIME writeTime;
//	if (!JsonLoader::GetLastWriteTime(m_filePathBuf, writeTime))
//	{
//		return;
//	}
//
//	if (m_lastWriteTime.dwLowDateTime == 0 && m_lastWriteTime.dwHighDateTime == 0)
//	{
//		m_lastWriteTime = writeTime;
//		return;
//	}
//
//	if (CompareFileTime(&writeTime, &m_lastWriteTime) == 0)
//	{
//		return;
//	}
//
//	m_lastWriteTime = writeTime;
//
//	int keepSelected = m_selected;
//
//	Load(m_filePathBuf);
//
//	if (keepSelected >= 0 && keepSelected < (int)m_objects.size())
//	{
//		m_selected = keepSelected;
//	}
//
//	KdDebugGUI::Instance().AddLog("EffectEditor: 外部変更を検知し自動リロードしました (%s)\n", m_filePathBuf);
//}