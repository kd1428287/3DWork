#pragma once
#include <cstdio>
#include <string>
#include <utility>
#include "../Transform/TransformComponent.h"
#include "../Camera/CameraComponent.h"

class SpriteRenderComponent : public ComponentBase, public IRenderable {
public:
	SpriteRenderComponent(GameObject* owner, std::string spriteName)
		: ComponentBase(owner), spriteName_(std::move(spriteName)) {}

	void DrawSprite() override {
		/*TransformComponent* trans = GetOwner()->GetComponent<TransformComponent>();
		if (!trans)return;
		KdShaderManager::Instance().m_spriteShader.SetMatrix(trans->matrix);
		KdShaderManager::Instance().m_spriteShader.DrawTex(
			KdAssets::Instance().m_textures.GetData(spriteName_).get(),0,0
		);*/
	}

private:
	std::string spriteName_;
};