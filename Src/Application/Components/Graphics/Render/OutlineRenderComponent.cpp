#include "OutlineRenderComponent.h"

OutlineRenderComponent::OutlineRenderComponent(GameObject* owner)
	: ComponentBase(owner)
{
}

// 同一GameObjectのTransformとモデル供給者を収集する(ModelRenderComponentと同じ方式)
void OutlineRenderComponent::Awake()
{
	transform_ = GetOwner()->GetComponent<TransformComponent>();

	auto sources = GetOwner()->GetTagged<IModelRenderSource>();
	modelSource_ = sources.empty() ? nullptr : sources.front();
}

// 表面を省略して裏面だけを、線幅ぶん膨らませて描く(輪郭は墨一色)
void OutlineRenderComponent::DrawLit()
{
	if (width_ <= 0.0f || !transform_ || !modelSource_) { return; }
	if (!modelSource_->IsModelDrawable()) { return; }

	KdModelWork* model = modelSource_->GetModel();
	if (!model) { return; }

	KdShaderManager& manager = KdShaderManager::Instance();

	manager.ChangeRasterizerState(KdRasterizerState::CullFront);

	// 線幅は描画後にDrawModel側で自動的に0へ戻る
	manager.m_StandardShader.SetOutlineWidth(width_);
	manager.m_StandardShader.DrawModel(*model, transform_->GetWorldMatrix());

	manager.UndoRasterizerState();
}
