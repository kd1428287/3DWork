#pragma once

// ※ ImGui / DirectXTK(SimpleMath) は既存のPCH等で読み込まれている前提です。
//
// 対象を選ばないグローバル系のシェーダーパラメータ(ポストプロセス・ライト・フォグ)を
// リアルタイムに調整するためのエディタです。
//
// マテリアル編集(オブジェクト単位)は今回のスコープ外のため、枠(TODO)のみ用意しています。
//
// KdDebugGUI::GuiProcess() の中(ImGui::NewFrame() 後 ～ ImGui::Render() 前)から
// Update() を呼び出して使用します。
class ShaderTuningEditor
{
public:

	// 毎フレームの更新・描画(ImGuiウィンドウ)
	void Update();

private:

	void DrawPostProcessPanel();
	void DrawLightPanel();
	void DrawFogPanel();

	// TODO: 選択中オブジェクトのマテリアル編集(今回は未実装、枠のみ)
	void DrawMaterialPanel();

	//=====================================================
	// ライト調整用の編集値
	//	Override が ON の間だけ、毎フレーム KdShaderManager へ書き込む
	//=====================================================
	bool			m_lightOverride = false;
	Math::Vector4	m_ambientLight = { 0.3f, 0.3f, 0.3f, 1.0f };
	Math::Vector3	m_dirLightDir = { 1, -1, 1 };
	Math::Vector3	m_dirLightColor = { 2.25f, 2.25f, 2.25f };

	//=====================================================
	// フォグ調整用の編集値
	//	Override が ON の間だけ、毎フレーム KdShaderManager へ書き込む
	//=====================================================
	bool			m_fogOverride = false;
	bool			m_distanceFogEnable = false;
	Math::Vector3	m_distanceFogColor = { 1.0f, 1.0f, 1.0f };
	float			m_distanceFogDensity = 0.001f;
	bool			m_heightFogEnable = false;
	Math::Vector3	m_heightFogColor = { 1.0f, 1.0f, 1.0f };
	float			m_heightFogTop = 5.0f;
	float			m_heightFogBottom = -5.0f;
	float			m_heightFogBeginDist = 0.0f;

	// 初回のみ、現在エンジンに設定されている値を編集値へ読み込むためのフラグ
	bool			m_initialized = false;

	//=====================================================
	// シングルトンパターン
	//=====================================================
private:
	ShaderTuningEditor() {}
	~ShaderTuningEditor() {}

public:
	static ShaderTuningEditor& Instance() {
		static ShaderTuningEditor instance;
		return instance;
	}
};
