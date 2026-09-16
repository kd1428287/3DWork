#include "Framework/KdFramework.h"

#include "GaugeBarRenderer.h"

void GaugeBarRenderer::Draw(const Math::Vector3& pos, const Math::Vector2& size, float ratio, const GaugeBarStyle& style)
{
	// ratioを安全な範囲にクランプ(HealthComponent等の値が万一範囲外でも壊れないように)
	float clampedRatio = std::clamp(ratio, 0.0f, 1.0f);

	int x = (int)pos.x;
	int y = (int)pos.y;
	int w = (int)size.x;
	int h = (int)size.y;

	// 左上基準で描画するため、pivotは{0,0}を指定
	const Math::Vector2 pivot = { 0.0f, 0.0f };

	//-------------------------------------------
	// 背景(枠)描画
	//-------------------------------------------
	{
		auto backTex = KdAssets::Instance().m_textures.GetData(style.BackTexName);
		KdShaderManager::Instance().m_spriteShader.DrawTex(backTex.get(), x, y, w, h, nullptr, &kWhiteColor, pivot);
	}

	//-------------------------------------------
	// フィル(中身)描画：FillRatioで左から右へ切り抜く
	//-------------------------------------------
	if (clampedRatio > 0.0f)
	{
		auto fillTex = KdAssets::Instance().m_textures.GetData(style.FillTexName);

		// フィル用の切り抜き比率をセット
		KdShaderManager::Instance().m_spriteShader.SetFillRatio(clampedRatio);

		KdShaderManager::Instance().m_spriteShader.DrawTex(fillTex.get(), x, y, w, h, nullptr, &kWhiteColor, pivot);

		// 【重要】ここでリセットしないと、次に同じシェーダーで描画される
		// 他のスプライト/フォントまで切り抜かれたままになってしまう
		KdShaderManager::Instance().m_spriteShader.SetFillRatio(1.0f);
	}
}