#include "TerrainFactory.h"
#include "../../PrefabFactory.h"

#include "Application/Components/Graphics/Render/ModelRenderComponent.h"
#include "Application/Components/Graphics/Render/Mod/ForceMaxDepthComponent.h"
#include "Application/Components/Graphics/Render/Mod/SkyDomeEdgeFadeComponent.h"
#include "Application/Components/Graphics/Animation/SkeletonComponent.h"

#include "Application/Components/Physics/Collision/ColliderComponent.h"
#include "Application/Components/Physics/Movement/FollowCameraComponent.h"

TerrainFactory::TerrainFactory() = default;

GameObject* TerrainFactory::CreateTerrain(ObjectManager& objectManager, int ownerTerrainId)
{
	auto* ground = objectManager.Instantiate("ground");
	auto* transform = ground->AddComponent<TransformComponent>();
	auto* model = ground->AddComponent<SkeletonComponent>();
	model->SetModelData("Asset/Models/Terrains/Ground/Terrain.gltf");
	auto* collider = ground->AddComponent<ColliderComponent>();
	auto* groundModel = ground->AddComponent<ModelRenderComponent>();
	transform->SetPosition({ 0.f,0.f,0.f });
	collider->AddBox("body", Math::Vector3(50.f, 0.5f, 50.f), Math::Vector3(0.f, -2.f, 0.f), ColliderCategory::Ground);

	return ground;
}

GameObject* TerrainFactory::CreateSkydome(ObjectManager& objectManager, int ownerTerrainId)
{
	auto* skydome = objectManager.Instantiate("skydome");
	auto* transform = skydome->AddComponent<TransformComponent>();
	transform->SetScale({ 10.0f,10.0f,10.0f });
	auto* model = skydome->AddComponent<SkeletonComponent>();
	model->SetModelData("Asset/Models/SkySphere/SkyDome.gltf");
	auto* renderer = skydome->AddComponent<ModelRenderComponent>();
	renderer->SetLayer(RenderLayer::DrawUnLit);
	transform->SetPosition({ 0.f,0.f,0.f });
	auto* follow = skydome->AddComponent<FollowCameraComponent>();
	follow->SetOffset(Math::Vector3{ 0,-25.f,0 });
	skydome->AddComponent<ForceMaxDepthComponent>();
	skydome->AddComponent<SkyDomeEdgeFadeComponent>()->SetFadeRange(-0.2f, 0.f);

	return skydome;
}

// MapEditorが保存したエンティティ1件からGameObjectを組み立てる(実体はPrefabFactory)。
GameObject* TerrainFactory::CreateFromData(ObjectManager& objectManager, const MapEntity& data)
{
	return PrefabFactory::Create(objectManager, data);
}
