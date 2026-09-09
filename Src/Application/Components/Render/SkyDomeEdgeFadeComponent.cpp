#include "Framework/KdFramework.h"
#include "SkyDomeEdgeFadeComponent.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// コンストラクタ：特別な初期化は不要
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
SkyDomeEdgeFadeComponent::SkyDomeEdgeFadeComponent(GameObject* owner)
	: ComponentBase(owner)
{
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// フェード範囲の設定：スカイドームの半径や配置に合わせて呼び出し元が指定する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SkyDomeEdgeFadeComponent::SetFadeRange(float bottomY, float topY)
{
	bottomY_ = bottomY;
	topY_ = topY;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ModelRenderComponentの描画直前に呼ばれる：フェード範囲をシェーダーへ転送
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SkyDomeEdgeFadeComponent::Apply(KdStandardShader& shader) const
{
	shader.SetSkyDomeEdgeFade(bottomY_, topY_);
}
