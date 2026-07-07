#pragma once
#include <cstdio>
#include <string>
#include <utility>
#include "../Transform/TransformComponent.h"

class ModelRenderComponent : public ComponentBase, public IRenderable {
public:
	ModelRenderComponent(GameObject* owner, std::string modelName)
		: ComponentBase(owner), modelName_(std::move(modelName)) {}

	void GenerateDepthMapFromLight() override {
		TransformComponent* trans = GetOwner()->GetComponent<TransformComponent>();
		KdShaderManager::Instance().m_StandardShader.DrawModel(
			*KdAssets::Instance().m_modeldatas.GetData(modelName_),
			trans->matrix
		);
	}

	void DrawLit() override {
		std::printf("  [LitPass] %s(%s) を陰影付きで描く\n",
			GetOwner()->GetName().c_str(), modelName_.c_str());
		GenerateDepthMapFromLight();
	}

private:
	std::string modelName_;
};