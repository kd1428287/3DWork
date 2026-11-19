#pragma once

#include <imgui.h>

#include "Application/Definitions/Effect/SlashTrailParams.h"
#include "../../Effect/SlashTrail/SlashTrailDispatcher.h"
#include "../Common/EditorPreviewViewport.h"
#include "../Common/FileWatcher.h"

// 編集対象の1トレイル定義(JSONの "Trails" の1要素に対応)
struct SlashTrailObject
{
	std::string			name = "SlashTrail";
	SlashTrailParams	params;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル定義エディタ(EffectEditorと同じ構成)
//	Update()    ：KdDebugGUI::GuiProcess()内(EffectEditor::Update()と同じ場所)から毎フレーム
//	RenderPreviewViewport()：3D描画パス側から毎フレーム1回(EffectEditorと同じ場所)
// プレビューは円弧状にTip/Baseを動かしてSlashTrailRecorderへ流し、専用Dispatcherで描画する。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class SlashTrailEditor
{
public:

	void Update();

	// プレビュー専用カメラ・バッファへ、再生中のトレイルを描画する
	void RenderPreviewViewport();

private:

	enum class PlayState { Idle, Swinging, Fading };

	// プレビューで剣を振る軌道(保存しない)。角度は平面内でX軸から測る
	struct SwingSettings
	{
		float	Duration = 0.25f;
		float	ArcStartDeg = 160.0f;
		float	ArcEndDeg = 20.0f;
		float	Radius = 1.0f;			// 回転中心から剣の根元(Base)まで
		float	SwordLength = 1.0f;		// Base〜Tip
		float	PlaneTiltDeg = 0.0f;	// 0=縦(XY平面) 90=横(XZ平面)
	};

	void DrawMainMenu();
	void DrawHierarchy();
	void DrawInspector();
	void DrawPreviewWindow();

	bool TextureCombo(const char* label, std::string& path);

	SlashTrailObject* Selected();

	void AddObject();
	void RemoveSelected();
	void CopySelected();

	void Save(const std::string& path);
	void Load(const std::string& path);
	void CheckHotReload();

	// 編集を記録する(未保存の間はホットリロードで上書きしない)
	void MarkEdited() { m_dirty = true; m_defStale = true; }

	void Play();
	void Stop();
	void UpdatePlayback(float deltaTime);

	std::vector<SlashTrailObject>	m_objects;
	int								m_selected = -1;

	char	m_filePathBuf[260] = "Asset/Data/Game/slashtrail.json";

	FileWatcher	m_watcher;
	bool		m_dirty = false;

	std::vector<std::string>	m_textureFileList;
	bool						m_textureListLoaded = false;

	EditorPreviewViewport	m_preview;

	// プレビュー再生(Init()は呼ばず、RegisterDefinition()でエディタ上の値を直接登録する)
	SlashTrailDispatcher	m_dispatcher;
	SlashTrailRecorder		m_recorder;
	SlashTrailHandle		m_handle;
	SlashTrailDefId			m_defId = kInvalidSlashTrailDef;
	bool					m_defStale = true;	// trueなら次のPlay()で定義を登録し直す

	SwingSettings	m_swing;
	PlayState		m_state = PlayState::Idle;
	float			m_time = 0.0f;
	float			m_fadeWait = 0.0f;
	bool			m_loop = true;

	//=====================================================
	// シングルトンパターン
	//=====================================================
private:
	SlashTrailEditor();
	~SlashTrailEditor() {}

public:
	static SlashTrailEditor& Instance() {
		static SlashTrailEditor instance;
		return instance;
	}
};
