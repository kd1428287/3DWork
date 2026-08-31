#pragma once
#include <string>
#include <algorithm>
#include "../Camera/CameraComponent.h" // KdShaderManagerのDrawTex/SetMatrix呼び出しパターンをSpriteRenderComponentに合わせるため

// ============================================================
// GaugeBarRenderer
//
// HPバー/体幹バーに共通する「背景 + 現在値割合(ratio)で幅を縮めた塗り」
// という描画パターンを切り出したユーティリティ。単純な引き伸ばし描画と
// 9スライス描画の両方に対応する。
//
// 【9スライスの実装方式について】
// 1枚のテクスチャをUV矩形で9分割する方式ではなく、四隅・上下・左右・
// 中央の9枚を個別のテクスチャ画像として用意し、それぞれをDrawQuad()で
// 貼り合わせる方式にしている。KdShaderManager::m_spriteShader.DrawTex()が
// 単一テクスチャのUVサブ矩形指定に対応しているか未確認のため、既に動作
// 確認済みのDrawQuad()(フルテクスチャ描画)だけで組める、この方式を
// 採用した。UVサブ矩形指定に対応していることが確認できれば、1枚絵+
// マージン指定の方式に置き換えたほうがテクスチャ管理は楽になる。
//
// 描画呼び出しはSpriteRenderComponentと同じ形
// (KdShaderManager::Instance().m_spriteShader.SetMatrix() → DrawTex())
// に合わせてある。色ではなくテクスチャ画像で表現する方式のため、
// 各テクスチャ名には無地(白 or 単色)、あるいは9スライス用の縁取り画像を
// 渡す想定。
//
// screenPosの座標系(画面左上原点か中心原点か、Y軸が下向きか上向きか)は、
// 実際に使っているUI用TransformComponent/スプライトシェーダーの前提に
// 合わせて呼び出し側(PlayerStatusUIComponent/EnemyHPBarComponent)で
// 調整すること。DrawNineSlice()内のtop/bottomの配置もこれに従うため、
// 実機で上下が逆に見える場合はDrawNineSlice()側のY軸の符号を反転すること。
// ============================================================

// 9スライスの四隅/辺/中央、9枚分のテクスチャ名。
// 空文字のパーツは描画をスキップする(例: 上下だけ縁取りたい場合、
// left/rightをtop/bottom用の画像と同じにする、center以外を空にする等、
// 自由に組み合わせられる)。
struct NineSliceTextures
{
	std::string topLeft, top, topRight;
	std::string left, center, right;
	std::string bottomLeft, bottom, bottomRight;
};

// 9スライスの余白(ピクセル単位)。四隅はこのサイズで固定され、
// 辺と中央がバー全体のサイズに合わせて伸縮する。
struct NineSliceMargins
{
	float left = 4.0f;
	float right = 4.0f;
	float top = 4.0f;
	float bottom = 4.0f;
};

struct GaugeBarStyle
{
	// 9スライスを使わない場合(useNineSliceBack/Fillがfalse)はこちらを使う。
	std::string backTexture = "UI/gauge_back"; // 空き部分の背景テクスチャ(無地想定)
	std::string fillTexture = "UI/gauge_fill";  // 現在値の塗りテクスチャ(無地想定)

	// trueにすると、対応するテクスチャ(backTexture/fillTexture)の代わりに
	// backNineSlice/fillNineSliceを使って9スライス描画する。
	bool useNineSliceBack = false;
	bool useNineSliceFill = false;
	NineSliceTextures backNineSlice;
	NineSliceTextures fillNineSlice;

	// 9スライスの余白。back/fillで共通の値を使う
	// (見た目の縁取りの太さを揃えたい、という一般的な用途を想定)。
	// back/fillで別の余白にしたくなったら、GaugeBarStyle自体を2つの
	// NineSliceMarginsを持つ形へ拡張すること。
	NineSliceMargins margins;
};

class GaugeBarRenderer
{
public:
	// screenPos: バー中心のスクリーン座標(ピクセル)
	// size: バー全体の幅・高さ(ピクセル)
	// ratio: 0.0〜1.0 (HealthComponent::GetRatio()等から渡す)
	static void Draw(const Math::Vector3& screenPos, const Math::Vector2& size, float ratio, const GaugeBarStyle& style)
	{
		ratio = std::clamp(ratio, 0.0f, 1.0f);

		// 背景(フルサイズ)
		if (style.useNineSliceBack) {
			DrawNineSlice(screenPos, size, style.margins, style.backNineSlice);
		}
		else {
			DrawQuad(screenPos, size, style.backTexture);
		}

		// 塗り: 幅だけratio分に縮め、左詰めになるよう中心座標を左へずらす
		Math::Vector2 fillSize = { size.x * ratio, size.y };
		Math::Vector3 fillPos = screenPos;
		fillPos.x -= (size.x - fillSize.x) * 0.5f;

		if (style.useNineSliceFill) {
			DrawNineSlice(fillPos, fillSize, style.margins, style.fillNineSlice);
		}
		else {
			DrawQuad(fillPos, fillSize, style.fillTexture);
		}
	}

private:
	static void DrawQuad(const Math::Vector3& pos, const Math::Vector2& size, const std::string& textureName)
	{
		if (textureName.empty() || size.x <= 0.0f || size.y <= 0.0f) return;

		// SpriteRenderComponent::DrawSprite()と全く同じ呼び出し形。
		// TransformComponentを経由せず直接行列を組んでいる点だけが異なる
		// (UIバーはGameObject単位ではなく、値のみを持つコンポーネントから
		// 直接描画したいため)。
		Math::Matrix world = Math::Matrix::CreateScale(size.x, size.y, 1.0f)
			* Math::Matrix::CreateTranslation(pos);

		KdShaderManager::Instance().m_spriteShader.SetMatrix(world);
		KdShaderManager::Instance().m_spriteShader.DrawTex(
			KdAssets::Instance().m_textures.GetData(textureName).get(), 0, 0
		);
	}

	// centerPosを中心とするsize(幅・高さ)の矩形を、9枚のパーツに分けて
	// 貼り合わせる。四隅はmarginsのサイズで固定し、辺・中央はsizeに合わせて
	// 伸縮させる。
	//
	// sizeがmarginsの合計より小さい(バーがほぼ0まで縮んだ)場合、四隅が
	// お互いに重なって見た目が破綻しないよう、この矩形に収まる範囲まで
	// marginsを一時的に縮小してから配置する。
	static void DrawNineSlice(const Math::Vector3& centerPos, const Math::Vector2& size,
		const NineSliceMargins& margins, const NineSliceTextures& tex)
	{
		if (size.x <= 0.0f || size.y <= 0.0f) return;

		const float left = std::min(margins.left, size.x * 0.5f);
		const float right = std::min(margins.right, size.x * 0.5f);
		const float top = std::min(margins.top, size.y * 0.5f);
		const float bottom = std::min(margins.bottom, size.y * 0.5f);

		const float midWidth = std::max(0.0f, size.x - left - right);
		const float midHeight = std::max(0.0f, size.y - top - bottom);

		// centerPos基準で左端・上端(この関数内だけのローカル基準)を求める。
		// 【要確認】Y軸が下向き(左上原点寄り)か上向き(中心原点寄り)かで
		// top/bottomの配置が逆に見える可能性がある。実機で確認し、
		// 逆であればy0の符号、あるいは各行のY位置を入れ替えること。
		const float x0 = centerPos.x - size.x * 0.5f;
		const float y0 = centerPos.y - size.y * 0.5f;

		const float colLeftX = x0 + left * 0.5f;
		const float colMidX = x0 + left + midWidth * 0.5f;
		const float colRightX = x0 + left + midWidth + right * 0.5f;

		const float rowTopY = y0 + top * 0.5f;
		const float rowMidY = y0 + top + midHeight * 0.5f;
		const float rowBottomY = y0 + top + midHeight + bottom * 0.5f;

		auto drawPart = [&](float x, float y, float w, float h, const std::string& texture) {
			DrawQuad(Math::Vector3(x, y, centerPos.z), Math::Vector2(w, h), texture);
			};

		drawPart(colLeftX, rowTopY, left, top, tex.topLeft);
		drawPart(colMidX, rowTopY, midWidth, top, tex.top);
		drawPart(colRightX, rowTopY, right, top, tex.topRight);

		drawPart(colLeftX, rowMidY, left, midHeight, tex.left);
		drawPart(colMidX, rowMidY, midWidth, midHeight, tex.center);
		drawPart(colRightX, rowMidY, right, midHeight, tex.right);

		drawPart(colLeftX, rowBottomY, left, bottom, tex.bottomLeft);
		drawPart(colMidX, rowBottomY, midWidth, bottom, tex.bottom);
		drawPart(colRightX, rowBottomY, right, bottom, tex.bottomRight);
	}
};