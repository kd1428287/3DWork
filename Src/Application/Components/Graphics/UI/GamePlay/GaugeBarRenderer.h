#pragma once
#include <algorithm>
#include "Application/Definitions/UI/GaugeBarStyle.h"

//===================================================
//
// ゲージバー(HP/体幹等)の見た目を描画するだけの
// ステートレスなユーティリティ
// ・座標/数値/テクスチャは呼び出し側からすべて渡してもらう
// 　ことで、画面固定UI/ワールド追従UIの両方から共通利用できる
//
//===================================================

// 読み込み済みのゲージ見た目。毎フレームのアセット検索を避けるためAwakeでLoadして使う
struct GaugeBarSkin
{
	std::shared_ptr<KdTexture> frameTex;
	std::shared_ptr<KdTexture> fillTex;
	int frameBorder = 0;

	void Load(const GaugeBarStyle& style)
	{
		frameTex    = KdAssets::Instance().m_textures.GetData(style.FrameTexName);
		fillTex     = KdAssets::Instance().m_textures.GetData(style.FillTexName);
		frameBorder = style.FrameBorder;
	}
};

struct GaugeBarRenderer
{
	// ゲージ1本を描画する(shaderはBegin～End間で呼ぶこと)
	// ・ratio		… 現在値の割合(範囲外は内部でクランプ)
	// ・fillTex	… nullptrなら何も描画しない
	// ・tint		… 枠と中身の両方に掛ける色(フェードのアルファもここ)
	// ・pivot		… KdSpriteShader::DrawTexと同じ意味
	// ・frameBorder … 枠の厚み(px)。中身は枠の内側にこの分だけインセットして描く
	static void Draw(
		KdSpriteShader& shader,
		const Math::Vector2& screenPos,
		const Math::Vector2& size,
		float ratio,
		const KdTexture* frameTex,
		const KdTexture* fillTex,
		const Math::Color& tint = kWhiteColor,
		const Math::Vector2& pivot = { 0.5f, 0.5f },
		int frameBorder = 0)
	{
		if (fillTex == nullptr) { return; }

		float clampedRatio = std::clamp(ratio, 0.0f, 1.0f);

		if (frameTex)
		{
			shader.DrawTex(frameTex, (int)screenPos.x, (int)screenPos.y, (int)size.x, (int)size.y, nullptr, &tint, pivot);
		}

		// 枠の厚みがバーより大きくても中身が反転しないよう上限を掛ける
		float border = std::clamp((float)frameBorder, 0.0f, (std::min(size.x, size.y) - 1.0f) * 0.5f);

		// 四方をborderずつ縮めた矩形を、同じpivotで置き直した座標(pivotの向きに依らず成立)
		Math::Vector2 fillSize = { size.x - border * 2.0f, size.y - border * 2.0f };
		Math::Vector2 fillPos  = {
			screenPos.x + border * (1.0f - pivot.x * 2.0f),
			screenPos.y + border * (1.0f - pivot.y * 2.0f) };

		// 【注意】FillRatioは必ず1.0fへ戻す。戻し忘れると後続スプライトまで切り抜かれる
		shader.SetFillRatio(clampedRatio);
		shader.DrawTex(fillTex, (int)fillPos.x, (int)fillPos.y, (int)fillSize.x, (int)fillSize.y, nullptr, &tint, pivot);
		shader.SetFillRatio(1.0f);
	}

	// Skin版:テクスチャと枠の厚みをまとめて渡す
	static void Draw(
		KdSpriteShader& shader,
		const Math::Vector2& screenPos,
		const Math::Vector2& size,
		float ratio,
		const GaugeBarSkin& skin,
		const Math::Color& tint = kWhiteColor,
		const Math::Vector2& pivot = { 0.5f, 0.5f })
	{
		Draw(shader, screenPos, size, ratio,
			skin.frameTex.get(), skin.fillTex.get(), tint, pivot, skin.frameBorder);
	}
};
