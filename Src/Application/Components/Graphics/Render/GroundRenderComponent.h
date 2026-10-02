#pragma once
#include "../../Tags/IRenderable.h"

// ============================================================
// y=0の無限地面。モデル非依存で、カメラXZに追従する巨大クアッドを描く。
// 実際の描画はKdStandardShader::DrawGround()に委譲する。
// ============================================================
class GroundRenderComponent : public ComponentBase, public IRenderable
{
public:
	explicit GroundRenderComponent(GameObject* owner);

	void DrawLit() override;

	// 地面マテリアル(baseColor/normal/metallicRoughnessのテクスチャをここに設定する)
	KdMaterial& WorkMaterial() { return material_; }

	// 1タイルのワールドサイズ
	void SetTileSize(float tileSize) { tileSize_ = tileSize; }
	// クアッド半径(フォグが飽和する距離以上、カメラfar以下にする)
	void SetHalfSize(float halfSize) { halfSize_ = halfSize; }

private:
	KdMaterial material_;
	float tileSize_ = 4.0f;
	float halfSize_ = 2000.0f;
};
