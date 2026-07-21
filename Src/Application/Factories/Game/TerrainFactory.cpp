#include "TerrainFactory.h"

#include "../../Components/Render/ModelRenderComponent.h"
#include "../../Components/Collision/ColliderComponent.h"

GameObject* TerrainFactory::CreateTerrain(ObjectManager& objectManager, int ownerTerrainId)
{
	auto* ground = objectManager.Instantiate("ground");
	auto* transform = ground->AddComponent<TransformComponent>();
	auto* collider = ground->AddComponent<ColliderComponent>();
	auto* groundModel = ground->AddComponent<ModelRenderComponent>(
		"Asset/Models/Terrains/Ground/Terrain.gltf"
	);
	transform->SetPosition({ 0.f,0.f,0.f });
	collider->AddBox("body", Math::Vector3(10.f, 1.f, 10.f), Math::Vector3(0.f,-2.f,0.f), ColliderLayer::Ground);

	return ground;
}
