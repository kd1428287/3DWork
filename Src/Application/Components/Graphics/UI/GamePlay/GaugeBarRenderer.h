#pragma once

// ゲージバー(HP/体幹等)の見た目だけを描画するステートレスなユーティリティ
struct GaugeBarRenderer
{
	// ゲージ1本を描画する(frameBorder>0で枠+背景一体テクスチャを9スライス)
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
			if (frameBorder > 0)
				DrawNineSliceFrame(shader, frameTex, screenPos, size, pivot, frameBorder);
			else
				shader.DrawTex(frameTex, (int)screenPos.x, (int)screenPos.y, (int)size.x, (int)size.y, nullptr, &kWhiteColor, pivot);
		}

		// FillRatioは呼ぶたびに必ず1.0fへ戻す(戻し忘れ防止)
		shader.SetFillRatio(clampedRatio);
		shader.DrawTex(fillTex, (int)screenPos.x, (int)screenPos.y, (int)size.x, (int)size.y, nullptr, &tint, pivot);
		shader.SetFillRatio(1.0f);
	}

private:
	// 3x3(角4+辺4+中央1)に分割し、角のサイズを保ったまま描画する
	static void DrawNineSliceFrame(
		KdSpriteShader& shader,
		const KdTexture* tex,
		const Math::Vector2& screenPos,
		const Math::Vector2& size,
		const Math::Vector2& pivot,
		int border)
	{
		const int texW = tex->GetInfo().Width;
		const int texH = tex->GetInfo().Height;

		const int left = (int)(screenPos.x - pivot.x * size.x);
		const int top = (int)(screenPos.y - pivot.y * size.y);
		const int w = (int)size.x;
		const int h = (int)size.y;

		const int srcColW[3] = { border, texW - border * 2, border };
		const int srcRowH[3] = { border, texH - border * 2, border };
		const int dstColW[3] = { border, std::max(0, w - border * 2), border };
		const int dstRowH[3] = { border, std::max(0, h - border * 2), border };

		int srcY = 0, dstY = top;
		for (int row = 0; row < 3; ++row)
		{
			int srcX = 0, dstX = left;
			for (int col = 0; col < 3; ++col)
			{
				Math::Rectangle srcRect(srcX, srcY, srcColW[col], srcRowH[row]);
				shader.DrawTex(tex, dstX, dstY, dstColW[col], dstRowH[row], &srcRect, &kWhiteColor, { 0.0f, 0.0f });
				srcX += srcColW[col];
				dstX += dstColW[col];
			}
			srcY += srcRowH[row];
			dstY += dstRowH[row];
		}
	}
};
