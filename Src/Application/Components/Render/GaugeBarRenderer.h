#pragma once
#include <string>
#include <algorithm>
#include "../Camera/CameraComponent.h" // KdShaderManagerのDrawTex/SetMatrix呼び出しパターンをSpriteRenderComponentに合わせるため

// ============================================================
// GaugeBarRenderer
//
// HPバー/体幹バーに共通する「背景クアッド + 現在値割合(ratio)で
// 幅を縮めた塗りクアッド」という描画パターンだけを切り出したユーティリティ。
//
// 描画呼び出しはSpriteRenderComponentと同じ形
// (KdShaderManager::Instance().m_spriteShader.SetMatrix() → DrawTex())
// に合わせてある。色ではなくテクスチャ画像で表現する方式のため、
// backTexture/fillTextureには無地(白 or 単色)のテクスチャ名を渡す想定。
//
// screenPosの座標系(画面左上原点か中心原点か)は、実際に使っている
// UI用TransformComponent/スプライトシェーダーの前提に合わせて
// 呼び出し側(PlayerStatusUIComponent/EnemyHPBarComponent)で調整すること。
// ============================================================

struct GaugeBarStyle
{
	std::string backTexture = "UI/gauge_back"; // 空き部分の背景テクスチャ(無地想定)
	std::string fillTexture = "UI/gauge_fill";  // 現在値の塗りテクスチャ(無地想定)
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
		DrawQuad(screenPos, size, style.backTexture);

		// 塗り: 幅だけratio分に縮め、左詰めになるよう中心座標を左へずらす
		Math::Vector2 fillSize = { size.x * ratio, size.y };
		Math::Vector3 fillPos = screenPos;
		fillPos.x -= (size.x - fillSize.x) * 0.5f;
		DrawQuad(fillPos, fillSize, style.fillTexture);
	}

private:
	static void DrawQuad(const Math::Vector3& pos, const Math::Vector2& size, const std::string& textureName)
	{
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
};