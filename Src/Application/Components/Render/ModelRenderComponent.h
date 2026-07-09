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

	void Start()override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	void GenerateDepthMapFromLight() override {
		if (!spModel_)return;
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