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
	// 単色バー(fillTexなし)の色。fillTexがある場合は白のままでよい
	Math::Color fillColor = kWhiteColor;
	// trueなら両端が半円のカプセル型画像として3分割描画する(画像は余白なしで切り詰めておく)
	bool capsule = false;

	void Load(const GaugeBarStyle& style)
	{
		frameTex = KdAssets::Instance().m_textures.GetData(style.FrameTexName);
		// FillTexNameが空なら、白テクスチャにfillColorを掛けた単色バーにする
		fillTex = style.FillTexName.empty()
			? KdDirect3D::Instance().GetWhiteTex()
			: KdAssets::Instance().m_textures.GetData(style.FillTexName);
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
	// ・fillColor	… 中身だけに掛ける色(単色バー用。枠には掛からない)
	static void Draw(
		KdSpriteShader& shader,
		const Math::Vector2& screenPos,
		const Math::Vector2& size,
		float ratio,
		const KdTexture* frameTex,
		const KdTexture* fillTex,
		const Math::Color& tint = kWhiteColor,
		const Math::Vector2& pivot = { 0.5f, 0.5f },
		int frameBorder = 0,
		const Math::Color& fillColor = kWhiteColor)
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
		Math::Vector2 fillPos = {
			screenPos.x + border * (1.0f - pivot.x * 2.0f),
			screenPos.y + border * (1.0f - pivot.y * 2.0f) };

		// 【注意】FillRatioは必ず1.0fへ戻す。戻し忘れると後続スプライトまで切り抜かれる
		shader.SetFillRatio(clampedRatio);
		const Math::Color fillTint = tint * fillColor;
		shader.DrawTex(fillTex, (int)fillPos.x, (int)fillPos.y, (int)fillSize.x, (int)fillSize.y, nullptr, &fillTint, pivot);
		shader.SetFillRatio(1.0f);
	}

	// カプセル型:枠と中身を同じ3分割(左半円/中央/右半円)で描く。
	// 半円は縦横比のまま、中央だけ横に伸ばす。中身の割合は各パーツのsrcRect/幅を詰めて切る(FillRatio不使用)。
	// frameTex/fillTexは同サイズ・余白なしの画像で、半円の半径は画像の高さ/2とする
	static void DrawCapsule(
		KdSpriteShader& shader,
		const Math::Vector2& screenPos,
		const Math::Vector2& size,
		float ratio,
		const KdTexture* frameTex,
		const KdTexture* fillTex,
		const Math::Color& tint = kWhiteColor,
		const Math::Vector2& pivot = { 0.5f, 0.5f },
		const Math::Color& fillColor = kWhiteColor)
	{
		const float x0 = screenPos.x - size.x * pivot.x;
		if (frameTex) { DrawSliced3(shader, frameTex, x0, screenPos.y, size, pivot.y, 1.0f, tint); }
		if (fillTex)
		{
			const Math::Color fillTint = tint * fillColor;
			DrawSliced3(shader, fillTex, x0, screenPos.y, size, pivot.y, std::clamp(ratio, 0.0f, 1.0f), fillTint);
		}
	}

	// Skin版:テクスチャと枠の厚み(カプセル型ならcapsule)をまとめて渡す
	static void Draw(
		KdSpriteShader& shader,
		const Math::Vector2& screenPos,
		const Math::Vector2& size,
		float ratio,
		const GaugeBarSkin& skin,
		const Math::Color& tint = kWhiteColor,
		const Math::Vector2& pivot = { 0.5f, 0.5f })
	{
		if (skin.capsule)
		{
			DrawCapsule(shader, screenPos, size, ratio,
				skin.frameTex.get(), skin.fillTex.get(), tint, pivot, skin.fillColor);
			return;
		}
		Draw(shader, screenPos, size, ratio,
			skin.frameTex.get(), skin.fillTex.get(), tint, pivot, skin.frameBorder, skin.fillColor);
	}

private:
	// 左端x0・縦位置y・大きさsizeの矩形へ3分割で描く。visibleRatioの分だけ左から表示する
	static void DrawSliced3(
		KdSpriteShader& shader,
		const KdTexture* tex,
		float x0, float y,
		const Math::Vector2& size,
		float pivotY,
		float visibleRatio,
		const Math::Color& color)
	{
		const int texW = (int)tex->GetWidth();
		const int texH = (int)tex->GetHeight();
		const int capSrc = texH / 2;

		// 幅が高さより狭い場合は半円が重ならないよう縮める
		const float capDest = std::min(capSrc * size.y / texH, size.x * 0.5f);

		const float dest[4] = { x0, x0 + capDest, x0 + size.x - capDest, x0 + size.x };
		const int   src[4] = { 0, capSrc, texW - capSrc, texW };
		const float clipX = x0 + size.x * visibleRatio;

		for (int i = 0; i < 3; ++i)
		{
			const float partW = dest[i + 1] - dest[i];
			if (partW <= 0.0f) { continue; }

			// このパーツの表示割合(0で非表示、1で全表示)
			const float r = std::clamp((clipX - dest[i]) / partW, 0.0f, 1.0f);
			if (r <= 0.0f) { continue; }

			const int dx = (int)std::lround(dest[i]);
			const int dw = (int)std::lround(dest[i] + partW * r) - dx;
			const int sw = std::max(1, (int)std::lround((src[i + 1] - src[i]) * r));
			if (dw <= 0) { continue; }

			const Math::Rectangle srcRect = { src[i], 0, sw, texH };
			shader.DrawTex(tex, dx, (int)y, dw, (int)size.y, &srcRect, &color, { 0.0f, pivotY });
		}
	}
};