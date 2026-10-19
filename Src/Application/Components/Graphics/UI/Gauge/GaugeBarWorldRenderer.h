#pragma once
#include "GaugeBarRenderer.h"

//===================================================
//
// ゲージをワールド空間のビルボードポリゴンで描く(深度テストで手前の物に隠れる)
// ・座標はビルボード平面上のメートル。topCenterはバー上辺の中央で、バーは下へ伸びる
// ・fillMode/capsuleはGaugeBarRenderer(スプライト版)と同じ見た目になるよう揃えてある
//
//===================================================
class GaugeBarWorldRenderer
{
public:
	// skinのLoad後に呼ぶ(テクスチャごとに板ポリを1枚作り、描画のたびに使い回す)
	void Load(const GaugeBarSkin& skin)
	{
		frameQuad_ = MakeQuad(skin.frameTex);
		fillQuad_ = MakeQuad(skin.fillTex);
	}

	// ・billboard		… カメラ向きのワールド行列(位置はゲージの基準点)
	// ・metersPerPixel	… frameBorder(px)をメートルへ換算する係数
	void Draw(
		KdStandardShader& shader,
		const Math::Matrix& billboard,
		const Math::Vector2& topCenter,
		const Math::Vector2& size,
		float ratio,
		const GaugeBarSkin& skin,
		const Math::Color& tint,
		float metersPerPixel)
	{
		const float r = std::clamp(ratio, 0.0f, 1.0f);
		const float x0 = topCenter.x - size.x * 0.5f;
		const float y = topCenter.y;
		const Math::Color fillTint = tint * skin.fillColor;

		const bool useCapsule = skin.capsule && skin.fillMode != GaugeFillMode::CenterOut;

		// 枠(全幅)
		if (useCapsule) { DrawSliced3(shader, billboard, frameQuad_, skin.frameTex.get(), x0, y, size, 1.0f, tint); }
		else { DrawQuad(shader, billboard, frameQuad_, x0, x0 + size.x, y, size.y, 0.0f, 1.0f, tint); }

		// 中身
		if (useCapsule)
		{
			DrawSliced3(shader, billboard, fillQuad_, skin.fillTex.get(), x0, y, size, r, fillTint);
			return;
		}

		// 枠の厚みの分だけ内側へ寄せる(バーより厚くならないよう上限を掛ける)
		const float maxBorder = std::max(0.0f, (std::min(size.x, size.y) - metersPerPixel) * 0.5f);
		const float border = std::clamp(skin.frameBorder * metersPerPixel, 0.0f, maxBorder);
		const float ix = x0 + border;
		const float iy = y - border;
		const float iw = size.x - border * 2.0f;
		const float ih = size.y - border * 2.0f;
		if (iw <= 0.0f || ih <= 0.0f) { return; }

		if (skin.fillMode == GaugeFillMode::CenterOut)
		{
			const float cx = ix + iw * 0.5f;
			const float hw = iw * 0.5f * r;
			DrawQuad(shader, billboard, fillQuad_, cx - hw, cx + hw, iy, ih, 0.5f - 0.5f * r, 0.5f + 0.5f * r, fillTint);
		}
		else
		{
			DrawQuad(shader, billboard, fillQuad_, ix, ix + iw * r, iy, ih, 0.0f, r, fillTint);
		}
	}

private:
	static KdSquarePolygon MakeQuad(const std::shared_ptr<KdTexture>& tex)
	{
		KdSquarePolygon quad(tex);
		quad.SetPivot(KdSquarePolygon::PivotType::Left_Top);	// 原点=左上、右(+x)と下(-y)へ伸びる
		return quad;
	}

	// x0～x1・上辺y・高さheightの矩形へ、テクスチャのu0～u1の範囲を描く
	static void DrawQuad(
		KdStandardShader& shader, const Math::Matrix& billboard, KdSquarePolygon& quad,
		float x0, float x1, float y, float height, float u0, float u1, const Math::Color& color)
	{
		if (!quad.GetMaterial() || x1 - x0 <= 0.0f || height <= 0.0f) { return; }

		quad.SetScale(Math::Vector2(x1 - x0, height));
		quad.SetUVRect(Math::Vector2(u0, 0.0f), Math::Vector2(u1, 1.0f));

		// DrawPolygonは描画後にオブジェクト定数を初期化するため、毎回フォグを切る
		shader.SetFogEnable(false);
		shader.DrawPolygon(quad, Math::Matrix::CreateTranslation(x0, y, 0.0f) * billboard, color);
	}

	// カプセル型:左半円/中央/右半円の3分割で、visibleRatioの分だけ左から描く
	static void DrawSliced3(
		KdStandardShader& shader, const Math::Matrix& billboard, KdSquarePolygon& quad, const KdTexture* tex,
		float x0, float y, const Math::Vector2& size, float visibleRatio, const Math::Color& color)
	{
		if (!tex || tex->GetWidth() == 0) { return; }

		// 半円の半径は画像の高さ/2。幅が狭いときは重ならないよう縮める
		const float cap = std::min(size.y * 0.5f, size.x * 0.5f);
		const float capU = (float)tex->GetHeight() / (2.0f * (float)tex->GetWidth());

		const float dest[4] = { x0, x0 + cap, x0 + size.x - cap, x0 + size.x };
		const float uv[4] = { 0.0f, capU, 1.0f - capU, 1.0f };
		const float clipX = x0 + size.x * visibleRatio;

		for (int i = 0; i < 3; ++i)
		{
			const float partW = dest[i + 1] - dest[i];
			if (partW <= 0.0f) { continue; }

			const float pr = std::clamp((clipX - dest[i]) / partW, 0.0f, 1.0f);
			if (pr <= 0.0f) { continue; }

			DrawQuad(shader, billboard, quad, dest[i], dest[i] + partW * pr, y, size.y,
				uv[i], uv[i] + (uv[i + 1] - uv[i]) * pr, color);
		}
	}

	KdSquarePolygon frameQuad_;
	KdSquarePolygon fillQuad_;
};
