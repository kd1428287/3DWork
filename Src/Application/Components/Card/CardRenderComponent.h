#pragma once
#include "../Render/MeshRenderComponent.h"
#include "../Render/SpriteRenderComponent.h"

//class CardRenderComponent : public MeshRenderComponent
class CardRenderComponent : public SpriteRenderComponent
{
public:
	CardRenderComponent(GameObject* owner)//, std::string spriteName)
		: SpriteRenderComponent(owner,"Asset/Textures/Game/Card/card.png") {}

};