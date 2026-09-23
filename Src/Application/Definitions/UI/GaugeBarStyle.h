#pragma once

// ゲージ1本分の見た目設定(枠+背景一体テクスチャ・中身テクスチャ・9スライス端幅)
struct GaugeBarStyle
{
	std::string FrameTexName;
	std::string FillTexName;
	int FrameBorder = 0;	// 0なら9スライスせず引き伸ばし

	GaugeBarStyle(const std::string& frameTex, const std::string& fillTex, int frameBorder = 0)
		: FrameTexName(frameTex), FillTexName(fillTex), FrameBorder(frameBorder) {}
};
