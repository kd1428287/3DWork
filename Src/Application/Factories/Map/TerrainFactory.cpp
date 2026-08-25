#include "TerrainFactory.h"
#include "../../Components/Render/ModelRenderComponent.h"
#include "../../Components/Animation/SkeletonComponent.h"
#include "../../Components/Collision/ColliderComponent.h"

GameObject* TerrainFactory::CreateFromData(ObjectManager& objectManager, const EntityData& data)
{
	auto* obj = objectManager.Instantiate(data.id);
	auto* transform = obj->AddComponent<TransformComponent>();
	auto* model = obj->AddComponent<SkeletonComponent>();
	auto* render = obj->AddComponent<ModelRenderComponent>();
	auto* collider = obj->AddComponent<ColliderComponent>();

	transform->SetPosition(data.transform.position);
	transform->SetScale(data.transform.scale);
	// ※必要に応じてオイラー角からクォータニオンへの変換を行う
	// transform->SetRotationEuler(data.transform.rotation);

	// アセットの割り当て
	if (data.assetId == "model_plane") {
		model->SetModelData("Asset/Models/Terrains/Ground/Terrain.gltf");
		if (data.colliderType == "Box") {
			collider->AddBox("body", Math::Vector3(50.f, 1.f, 50.f), Math::Vector3(0.f, -0.5f, 0.f), ColliderCategory::Ground);
		}
	}
	else if (data.assetId == "model_tree_pine") {
		model->SetModelData("Asset/Models/Props/Tree/Tree_Pine.gltf");
		// Cylinder未実装の場合はBox等で代用
	}

	return obj;
}