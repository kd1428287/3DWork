#include "OutlineRenderComponent.h"

OutlineRenderComponent::OutlineRenderComponent(GameObject* owner)
	: ComponentBase(owner)
{
	// 個体ごとに1〜255を割り当てる(隣接するキャラの輪郭が互いに消えないように)
	static UINT counter = 0;
	stencilRef_ = (counter++ % 255) + 1;
}

// 同一GameObjectのTransformとモデル供給者を収集する(ModelRenderComponentと同じ方式)
void OutlineRenderComponent::Awake()
{
	transform_ = GetOwner()->GetComponent<TransformComponent>();

	auto sources = GetOwner()->GetTagged<IModelRenderSource>();
	modelSource_ = sources.empty() ? nullptr : sources.front();
}

// ①シルエットをステンシルに書く ②膨らませた裏面を、シルエット外だけに描く(輪郭は墨一色)
void OutlineRenderComponent::DrawLit()
{
	if (width_ <= 0.0f || !transform_ || !modelSource_) { return; }
	if (!modelSource_->IsModelDrawable()) { return; }

	KdModelWork* model = modelSource_->GetModel();
	if (!model) { return; }

	KdShaderManager& manager = KdShaderManager::Instance();

	const Math::Matrix& world = transform_->GetWorldMatrix();

	// ①通常サイズで色なしの描画をし、シルエットにstencilRef_を書く
	manager.ChangeBlendState(KdBlendState::NoColorWrite);
	manager.ChangeDepthStencilState(KdDepthStencilState::StencilWrite, stencilRef_);
	manager.m_StandardShader.DrawModel(*model, world);
	manager.UndoDepthStencilState();
	manager.UndoBlendState();

	// ②シルエット外だけに、前面カリングの膨らませた描画をする(深度も書き、DoF等で背景扱いにならないようにする)
	manager.ChangeDepthStencilState(KdDepthStencilState::StencilOutlineZWrite, stencilRef_);
	manager.ChangeRasterizerState(KdRasterizerState::CullFront);

	// 線幅は描画後にDrawModel側で自動的に0へ戻る
	manager.m_StandardShader.SetOutlineWidth(width_);
	manager.m_StandardShader.DrawModel(*model, world);

	manager.UndoRasterizerState();
	manager.UndoDepthStencilState();
}
