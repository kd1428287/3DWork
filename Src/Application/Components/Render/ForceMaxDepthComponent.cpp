#include "Framework/KdFramework.h"
#include "ForceMaxDepthComponent.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// コンストラクタ：特別な初期化は不要
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
ForceMaxDepthComponent::ForceMaxDepthComponent(GameObject* owner)
	: ComponentBase(owner)
{
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ModelRenderComponentの描画直前に呼ばれる：常にForceMaxDepthを有効化する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ForceMaxDepthComponent::Apply(KdStandardShader& shader) const
{
	shader.SetForceMaxDepth(true);
}
