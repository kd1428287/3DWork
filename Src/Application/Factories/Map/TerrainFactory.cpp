#include "TerrainFactory.h"
#include "ComponentTypes.h"

#include <iostream>

#include "../../Components/Render/ModelRenderComponent.h"
#include "../../Components/Render/ForceMaxDepthComponent.h"
#include "../../Components/Render/SkyDomeEdgeFadeComponent.h"
#include "../../Components/Animation/SkeletonComponent.h"
#include "../../Components/Collision/ColliderComponent.h"
#include "../../Components/Movement/FollowCameraComponent.h"

TerrainFactory::TerrainFactory()
{
	// MapEditorを経由せずゲームを直接起動した場合でもComponentRegistryが
	// 空にならないよう、ここでも登録を試みる(2重登録は内部でガードされる)
	RegisterMapComponentTypes();
}

GameObject* TerrainFactory::CreateTerrain(ObjectManager& objectManager, int ownerTerrainId)
{
	auto* ground = objectManager.Instantiate("ground");
	auto* transform = ground->AddComponent<TransformComponent>();
	auto* model = ground->AddComponent<SkeletonComponent>();
	model->SetModelData("Asset/Models/Terrains/Ground/Terrain.gltf");
	auto* collider = ground->AddComponent<ColliderComponent>();
	auto* groundModel = ground->AddComponent<ModelRenderComponent>();
	transform->SetPosition({ 0.f,0.f,0.f });
	collider->AddBox("body", Math::Vector3(100.f, 1.f, 100.f), Math::Vector3(0.f, -2.f, 0.f), ColliderCategory::Ground);

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

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// MapEditorが保存したエンティティ1件からGameObjectを組み立てる。
//	コンポーネントの種類ごとの分岐は一切持たず、ComponentRegistryに登録された
//	create関数を呼ぶだけ。新しいコンポーネント種類を追加してもこの関数は無改修でよい
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
GameObject* TerrainFactory::CreateFromData(ObjectManager& objectManager, const EntityData& data)
{
	auto* obj = objectManager.Instantiate(data.name);
	auto* transform = obj->AddComponent<TransformComponent>();

	transform->SetPosition(data.transform.position);
	transform->SetScale(data.transform.scale);

	// オイラー角(度)→クォータニオン変換
	// ※ TransformComponentの回転セッターの実際のシグネチャが不明なため、
	//   一旦Quaternion版を想定して実装しています。実際のヘッダに合わせて調整してください
	Math::Quaternion rot = Math::Quaternion::CreateFromYawPitchRoll(
		DirectX::XMConvertToRadians(data.transform.rotation.y),
		DirectX::XMConvertToRadians(data.transform.rotation.x),
		DirectX::XMConvertToRadians(data.transform.rotation.z));
	transform->SetRotation(rot);

	for (const auto& entry : data.components)
	{
		const ComponentTypeInfo* info = ComponentRegistry::Instance().Find(entry.type);
		if (!info)
		{
			std::cerr << "[TerrainFactory] Unknown component type: " << entry.type
				<< " (entity: " << data.name << ")" << std::endl;
			continue;
		}

		info->create(*obj, entry.params);
	}

	return obj;
}