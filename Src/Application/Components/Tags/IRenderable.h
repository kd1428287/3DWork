#pragma once

// 描画用インターフェース。
namespace RenderLayer
{
	constexpr uint8_t PreDraw					= 1u << 0; 
	constexpr uint8_t GenerateDepthMapFromLight = 1u << 1; 
	constexpr uint8_t DrawUnLit					= 1u << 2; 
	constexpr uint8_t DrawLit					= 1u << 3; 
	constexpr uint8_t DrawEffect				= 1u << 4; 
	constexpr uint8_t DrawBright				= 1u << 5; 
	constexpr uint8_t DrawSprite				= 1u << 6; 
	constexpr uint8_t DrawDebug					= 1u << 7; 
}

class IRenderable {
public:
	virtual ~IRenderable() = default;

	virtual void PreDraw() {};
	// 光を遮るオブジェクト(影を落とす側)としてシャドウマップに描く
	virtual void GenerateDepthMapFromLight() {}
	// 陰影のないオブジェクト(背景など)
	virtual void DrawUnLit() {}
	// 陰影のあるオブジェクト(光源の影響を受ける)
	virtual void DrawLit() {}
	// エフェクト(陰影なし)
	virtual void DrawEffect() {}
	// 自ら光るオブジェクト・ブルーム対象
	virtual void DrawBright() {}
	// 2Dスプライト
	virtual void DrawSprite() {}
	// デバッグ描画
	virtual void DrawDebug() {}
};