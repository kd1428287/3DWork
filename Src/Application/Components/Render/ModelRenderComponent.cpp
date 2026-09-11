#include "Framework/KdFramework.h"
#include "ModelRenderComponent.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// コンストラクタ：特別な初期化は不要
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
ModelRenderComponent::ModelRenderComponent(GameObject* owner)
	: ComponentBase(owner)
{
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 兄弟コンポーネントの解決：モデル供給者(IModelRenderSource)と
// 個別描画パラメータ適用者(IRenderStateModifier)を、それぞれ
// タグレジストリ経由で1度だけ収集しておく
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ModelRenderComponent::Start()
{
	transform_ = GetOwner()->GetComponent<TransformComponent>();

	// モデル供給者を探す(SkeletonComponent、StaticModelComponent等)。
	// 同一GameObjectに複数付いているのは設定ミスの可能性が高いため検知しておく。
	auto sources = GetOwner()->GetTagged<IModelRenderSource>();
	assert(sources.size() <= 1 && "ModelRenderComponent: 複数のIModelRenderSourceが見つかりました");
	modelSource_ = sources.empty() ? nullptr : sources.front();

	// 個別の描画パラメータ適用コンポーネントを収集(毎フレームは走査しない)
	modifiers_ = GetOwner()->GetTagged<IRenderStateModifier>();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 各描画パス：dispatchのみ。パラメータ判断は一切持たない
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ModelRenderComponent::GenerateDepthMapFromLight()
{
	if (layer_ & RenderLayer::GenerateDepthMapFromLight) { DrawModel(); }
}

void ModelRenderComponent::DrawUnLit()
{
	if (layer_ & RenderLayer::DrawUnLit) { DrawModel(); }
}

void ModelRenderComponent::DrawLit()
{
	KdShaderManager::Instance().ChangeRasterizerState(KdRasterizerState::CullNone);
	if (layer_ & RenderLayer::DrawLit) { DrawModel(); }
}

void ModelRenderComponent::DrawEffect()
{
	if (layer_ & RenderLayer::DrawEffect) { DrawModel(); }
}

void ModelRenderComponent::DrawBright()
{
	if (layer_ & RenderLayer::DrawBright) { DrawModel(); }
}

void ModelRenderComponent::DrawSprite()
{
	if (layer_ & RenderLayer::DrawSprite) { DrawModel(); }
}

void ModelRenderComponent::DrawDebug()
{
	if (layer_ & RenderLayer::DrawDebug) { DrawModel(); }
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 実際の描画本体：
//  1. モデル供給者からモデルを取得(未設定・未準備なら何もしない)
//  2. 収集済みの各IRenderStateModifierを適用(このオブジェクト固有の演出)
//  3. KdStandardShaderへ描画を委譲
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ModelRenderComponent::DrawModel()
{
	if (!transform_ || !modelSource_) { return; }
	if (!modelSource_->IsModelDrawable()) { return; }

	KdModelWork* model = modelSource_->GetModel();
	if (!model) { return; }

	KdStandardShader& shader = KdShaderManager::Instance().m_StandardShader;

	for (const IRenderStateModifier* modifier : modifiers_)
	{
		modifier->Apply(shader);
	}

	shader.DrawModel(*model, transform_->GetWorldMatrix());
}
