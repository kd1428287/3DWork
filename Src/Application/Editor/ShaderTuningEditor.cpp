#include "ShaderTuningEditor.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ShaderTuningEditor::Update()
{
	// 初回だけ、現在エンジンに設定されている値をエディタの編集値へ読み込む
	// (Overrideをまだ入れていない状態でも、既存の値を初期表示できるようにするため)
	if (!m_initialized)
	{
		const auto& lightCB = KdShaderManager::Instance().GetLightCB();
		m_ambientLight  = lightCB.AmbientLight;
		m_dirLightDir   = lightCB.DirLight_Dir;
		m_dirLightColor = lightCB.DirLight_Color;

		const auto& fogCB = KdShaderManager::Instance().GetFogCB();
		m_distanceFogEnable  = fogCB.DistanceFogEnable != 0;
		m_distanceFogColor   = fogCB.DistanceFogColor;
		m_distanceFogDensity = fogCB.DistanceFogDensity;
		m_heightFogEnable    = fogCB.HeightFogEnable != 0;
		m_heightFogColor     = fogCB.HeightFogColor;
		m_heightFogTop       = fogCB.HeightFogTopValue;
		m_heightFogBottom    = fogCB.HeightFogBottomValue;
		m_heightFogBeginDist = fogCB.HeightFogBeginDistance;

		m_initialized = true;
	}

	DrawPostProcessPanel();
	DrawLightPanel();
	DrawFogPanel();
	DrawMaterialPanel();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ポストプロセス調整(対象を選ばない画面全体エフェクト)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ShaderTuningEditor::DrawPostProcessPanel()
{
	ImGui::Begin("Shader Tuning");

	if (ImGui::CollapsingHeader("Post Process", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto& post = KdShaderManager::Instance().m_postProcessShader;

		ImGui::Text("Color Grade");
		{
			const auto& cb = post.GetColorGradeCB();

			float exposure    = cb.Exposure;
			float contrast    = cb.Contrast;
			float saturation  = cb.Saturation;
			float temperature = cb.Temperature;
			float tint        = cb.Tint;

			if (ImGui::DragFloat("Exposure##pp", &exposure, 0.01f, 0.0f, 4.0f))     post.SetExposure(exposure);
			if (ImGui::DragFloat("Contrast##pp", &contrast, 0.01f, 0.0f, 4.0f))     post.SetContrast(contrast);
			if (ImGui::DragFloat("Saturation##pp", &saturation, 0.01f, 0.0f, 4.0f)) post.SetSaturation(saturation);
			if (ImGui::SliderFloat("Temperature##pp", &temperature, -1.0f, 1.0f))   post.SetTemperature(temperature);
			if (ImGui::SliderFloat("Tint##pp", &tint, -1.0f, 1.0f))                 post.SetTint(tint);
		}

		ImGui::Separator();
		ImGui::Text("Bloom");
		{
			const auto& cb = post.GetBrightCB();

			float threshold = cb.Threshold;
			if (ImGui::DragFloat("Threshold##bloom", &threshold, 0.01f, 0.0f, 10.0f)) post.SetBrightThreshold(threshold);
		}

		ImGui::Separator();
		ImGui::Text("Depth of Field");
		{
			const auto& cb = post.GetDoFCB();

			float nearClip  = cb.NearClippingDistance;
			float farClip   = cb.FarClippingDistance;
			float focusDist = cb.FocusDistance;
			float foreRange = cb.FocusForeRange;
			float backRange = cb.FocusBackRange;

			if (ImGui::DragFloat("Near Clip##dof", &nearClip, 0.1f))       post.SetNearClippingDistance(nearClip);
			if (ImGui::DragFloat("Far Clip##dof", &farClip, 0.5f))         post.SetFarClippingDistance(farClip);
			if (ImGui::DragFloat("Focus Distance##dof", &focusDist, 0.1f)) post.SetFocusDistance(focusDist);

			bool rangeChanged = false;
			rangeChanged |= ImGui::DragFloat("Focus Fore Range##dof", &foreRange, 0.1f, 0.0f, 1000.0f);
			rangeChanged |= ImGui::DragFloat("Focus Back Range##dof", &backRange, 0.1f, 0.0f, 1000.0f);
			if (rangeChanged) post.SetFocusRange(foreRange, backRange);
		}
	}

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ライト調整(対象を選ばない、シーン全体のライティング)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ShaderTuningEditor::DrawLightPanel()
{
	ImGui::Begin("Shader Tuning");

	if (ImGui::CollapsingHeader("Light"))
	{
		ImGui::Checkbox("Override##light", &m_lightOverride);
		ImGui::SameLine();
		ImGui::TextDisabled("(ONの間、毎フレームこの値で上書きします)");

		ImGui::ColorEdit3("Ambient##light", &m_ambientLight.x);

		ImGui::Separator();
		ImGui::Text("Directional Light");
		ImGui::DragFloat3("Direction##light", &m_dirLightDir.x, 0.01f);
		ImGui::ColorEdit3("Color(HDR)##light", &m_dirLightColor.x,
			ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);

		if (m_lightOverride)
		{
			KdShaderManager::Instance().WriteCBAmbientLight(m_ambientLight);
			KdShaderManager::Instance().WriteCBDirectionalLight(m_dirLightDir, m_dirLightColor);
		}
	}

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// フォグ調整(対象を選ばない画面全体の環境効果)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ShaderTuningEditor::DrawFogPanel()
{
	ImGui::Begin("Shader Tuning");

	if (ImGui::CollapsingHeader("Fog"))
	{
		ImGui::Checkbox("Override##fog", &m_fogOverride);

		ImGui::Checkbox("Distance Fog Enable", &m_distanceFogEnable);
		ImGui::ColorEdit3("Distance Fog Color", &m_distanceFogColor.x);
		ImGui::DragFloat("Distance Fog Density", &m_distanceFogDensity, 0.0001f, 0.0f, 1.0f, "%.5f");

		ImGui::Separator();
		ImGui::Checkbox("Height Fog Enable", &m_heightFogEnable);
		ImGui::ColorEdit3("Height Fog Color", &m_heightFogColor.x);
		ImGui::DragFloat("Height Fog Top", &m_heightFogTop, 0.1f);
		ImGui::DragFloat("Height Fog Bottom", &m_heightFogBottom, 0.1f);
		ImGui::DragFloat("Height Fog Begin Distance", &m_heightFogBeginDist, 0.1f);

		if (m_fogOverride)
		{
			KdShaderManager::Instance().WriteCBFogEnable(m_distanceFogEnable, m_heightFogEnable);
			KdShaderManager::Instance().WriteCBDistanceFog(m_distanceFogColor, m_distanceFogDensity);
			KdShaderManager::Instance().WriteCBHeightFog(m_heightFogColor, m_heightFogTop, m_heightFogBottom, m_heightFogBeginDist);
		}
	}

	ImGui::End();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マテリアル調整：今回は枠のみ(TODO)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ShaderTuningEditor::DrawMaterialPanel()
{
	ImGui::Begin("Shader Tuning");

	if (ImGui::CollapsingHeader("Material"))
	{
		ImGui::TextDisabled("TODO: 選択中オブジェクトのマテリアル編集(未実装)");
	}

	ImGui::End();
}
