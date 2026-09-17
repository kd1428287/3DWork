#pragma once

// ゲージの見た目設定(背景/フィルのテクスチャ名)
struct GaugeBarStyle
{
	std::string BackTexName;	// 背景(枠)テクスチャ名
	std::string FillTexName;	// フィル(中身)テクスチャ名

	GaugeBarStyle(const std::string& backTex, const std::string& fillTex)
		: BackTexName(backTex), FillTexName(fillTex) {}
};

// ============================================================
// GaugeBarRenderer
//
// HP/体幹などのゲージバーをスクリーン座標に描画する静的ユーティリティ。
// 背景(枠)テクスチャを指定サイズへ描画した上に、フィル(中身)テクスチャを
// KdSpriteShader::SetFillRatio()による切り抜きを使って重ね描画する。
//
// 現状は背景を単純に引き伸ばして描画するのみで、9スライスによる
// 角の保持には対応していない(必要になれば別途拡張する)。
// ============================================================
class GaugeBarRenderer
{
public:
	// ゲージバーを描画する
	// ・pos			… 画面座標(左上基準)
	// ・size			… 描画サイズ(幅・高さ、ピクセル)
	// ・ratio			… 0.0～1.0のゲージ割合(HealthComponent::GetRatio()等)
	// ・style			… 背景/フィルのテクスチャ名
	static void Draw(const Math::Vector3& pos, const Math::Vector2& size, float ratio, const GaugeBarStyle& style);
};
