#pragma once
#include <cstdio>
#include <string>
#include <utility>
#include "../Transform/TransformComponent.h"

class ModelRenderComponent : public ComponentBase, public IRenderable {
public:
	ModelRenderComponent(GameObject* owner, std::string modelName)
		: ComponentBase(owner)
	{
		spModel_ = std::make_shared<KdModelWork>(
			KdAssets::Instance().m_modeldatas.GetData(modelName)
		);
	}

	void SetModel(std::string modelName)
	{
		spModel_ = std::make_shared<KdModelWork>(
			KdAssets::Instance().m_modeldatas.GetData(modelName)
		);
	}

	void SetModel(const std::shared_ptr<KdModelData>& model)
	{
		spModel_ = std::make_shared<KdModelWork>(model);
	}

	void SetModel(const std::shared_ptr<KdModelWork>& model)
	{
		spModel_ = model;
	}

	void Start()override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	void GenerateDepthMapFromLight() override {
		if (!spModel_ || !transform_)return;
		//TransformComponent* trans = GetOwner()->GetComponent<TransformComponent>();
		KdShaderManager::Instance().m_StandardShader.DrawModel(
			*spModel_,
			transform_->GetWorldMatrix()
		);
	}

	void DrawLit() override {
		GenerateDepthMapFromLight();
	}

private:
	TransformComponent* transform_ = nullptr;
	std::shared_ptr<KdModelWork> spModel_ = nullptr;
};