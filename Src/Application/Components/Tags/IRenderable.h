#pragma once

// 描画用インターフェース

// どのレイヤーで描画するかのビットフラグ
//namespace RenderLayer
//{
//	constexpr uint8_t PreDraw					= 1u << 0; 
//	constexpr uint8_t GenerateDepthMapFromLight = 1u << 1; 
//	constexpr uint8_t DrawUnLit					= 1u << 2; 
//	constexpr uint8_t DrawLit					= 1u << 3; 
//	constexpr uint8_t DrawEffect				= 1u << 4; 
//	constexpr uint8_t DrawBright				= 1u << 5; 
//	constexpr uint8_t DrawSprite				= 1u << 6; 
//	//constexpr uint8_t DrawDebug					= 1u << 7; 
//	constexpr uint8_t DrawCharaShadow			= 1u << 7; 
//}

enum class RenderLayer : uint16_t
{
	PreDraw = 1u << 0,
	GenerateDepthMapFromLight = 1u << 1,
	DrawUnLit = 1u << 2,
	DrawLit = 1u << 3,
	DrawEffect = 1u << 4,
	DrawBright = 1u << 5,
	DrawSprite = 1u << 6,
	DrawDebug = 1u << 7,
	DrawCharaShadow = 1u << 8,
};

// --- ビット演算子のオーバーロード ---
inline constexpr RenderLayer operator|(RenderLayer lhs, RenderLayer rhs) {
	using T = std::underlying_type_t<RenderLayer>;
	return static_cast<RenderLayer>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

inline constexpr RenderLayer operator&(RenderLayer lhs, RenderLayer rhs) {
	using T = std::underlying_type_t<RenderLayer>;
	return static_cast<RenderLayer>(static_cast<T>(lhs) & static_cast<T>(rhs));
}

inline constexpr RenderLayer operator^(RenderLayer lhs, RenderLayer rhs) {
	using T = std::underlying_type_t<RenderLayer>;
	return static_cast<RenderLayer>(static_cast<T>(lhs) ^ static_cast<T>(rhs));
}

inline constexpr RenderLayer operator~(RenderLayer rhs) {
	using T = std::underlying_type_t<RenderLayer>;
	return static_cast<RenderLayer>(~static_cast<T>(rhs));
}

inline constexpr RenderLayer& operator|=(RenderLayer& lhs, RenderLayer rhs) {
	lhs = lhs | rhs;
	return lhs;
}

inline constexpr RenderLayer& operator&=(RenderLayer& lhs, RenderLayer rhs) {
	lhs = lhs & rhs;
	return lhs;
}

inline constexpr RenderLayer& operator^=(RenderLayer& lhs, RenderLayer rhs) {
	lhs = lhs ^ rhs;
	return lhs;
}

// フラグ判定を安全に行うためのヘルパー関数（任意）
inline constexpr bool HasFlag(RenderLayer layer, RenderLayer flag) {
	using T = std::underlying_type_t<RenderLayer>;
	return (static_cast<T>(layer) & static_cast<T>(flag)) != 0;
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
	// キャラ影
	virtual void DrawCharaShadow() {}
};