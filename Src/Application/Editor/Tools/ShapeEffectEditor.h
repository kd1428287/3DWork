#pragma once

#include <imgui.h>
#include "Application/Definitions/Loaders/ShapeEffectDataLoader.h"
#include "../../Effect/Shape/ShapeEffectInstance.h"
#include "../../Effect/Common/KdAssetsTextureProvider.h"
#include "../Common/EditorPreviewViewport.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 一枚テクスチャエフェクト(ShapeEffect)のエディタ
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 専用のコンテナウィンドウ("ShapeEffect Editor Window")のドックスペースへ、以下を初期配置する
// (EffectEditorと同じ作り。左：List / 中央：Inspector / 右：Preview(上)とPlayback(下))
// ・"ShapeEffect List"：ファイル操作・エフェクト一覧
// ・"ShapeEffect Inspector"：パラメータ・キー列
// ・"ShapeEffect Playback"：再生操作
// ・"ShapeEffect Preview"：専用カメラのプレビュー(EditorPreviewViewportを共有)
// 保存はShapeEffectDataLoader経由(effectmap.jsonとは別ファイル)。保存後は実行中の
// ShapeEffectDispatcherへ再ロードを通知する。
// プレビューは時間をエディタ側で持ち、ShapeEffectInstanceへ毎フレームSetTime()する
// (再生バーでの任意時刻へのシークが、そのまま動きの評価になる)。
//
// 呼び出し：EditorHost::Draw()からUpdate()、EditorHost::RenderPreviewViewports()から
// RenderPreviewViewport()。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class ShapeEffectEditor
{
public:

	void Update();

	void RenderPreviewViewport();

private:

	void DrawListWindow();
	void DrawInspectorWindow();
	void DrawPlaybackWindow();
	bool DrawParams(ShapeEffectData& effect);
	bool DrawTexturePicker(ShapeEffectParams& params);
	bool DrawKeys(ShapeEffectData& effect);
	void DrawPlayback();
	void DrawPreviewWindow();

	void AddEffect();
	void RemoveSelected();
	void CopySelected();
	void SelectEffect(int index);

	bool HasSelection() const { return m_selected >= 0 && m_selected < (int)m_effects.size(); }
	bool IsNameDuplicated(const std::string& name) const;
	std::string MakeUniqueName(const std::string& base) const;

	void Save(const std::string& path);
	void Load(const std::string& path);

	void RefreshTextureFileList();

	// 選択中のエフェクトからプレビュー用のモーションとインスタンスを作り直す(編集のたびに呼ぶ)
	void RebuildPreview();

	void AdvancePreview(float deltaTime);

	// 基準点：m_baseYawSpeedDeg=0なら固定、それ以外はY軸回りに回転する
	Math::Matrix GetPreviewBase() const;

	std::vector<ShapeEffectData>	m_effects;
	int								m_selected = -1;

	// プレビュー
	std::shared_ptr<KeyframeMotion>	m_motion;
	ShapeEffectInstance				m_previewInstance;
	float							m_time = 0.0f;
	bool							m_playing = false;
	bool							m_loop = true;
	float							m_baseYawSpeedDeg = 0.0f;

	KdAssetsTextureProvider			m_textureProvider;
	EditorPreviewViewport			m_preview;

	char	m_filePathBuf[260] = "Asset/Data/Game/shapeeffectmap.json";

	std::vector<std::string>		m_textureFileList;
	bool							m_textureListLoaded = false;

	//=====================================================
	// シングルトンパターン
	//=====================================================
private:
	ShapeEffectEditor();
	~ShapeEffectEditor() {}

public:
	static ShapeEffectEditor& Instance() {
		static ShapeEffectEditor instance;
		return instance;
	}
};