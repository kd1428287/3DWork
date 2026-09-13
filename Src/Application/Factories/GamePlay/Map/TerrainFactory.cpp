#include "TerrainFactory.h"
#include "Application/Definitions/Map/ComponentTypes.h"

#include "Application/Components/Graphics/Render/ModelRenderComponent.h"
#include "Application/Components/Graphics/Render/Mod/ForceMaxDepthComponent.h"
#include "Application/Components/Graphics/Render/Mod/SkyDomeEdgeFadeComponent.h"
#include "Application/Components/Graphics/Animation/SkeletonComponent.h"

#include "Application/Components/Physics/Collision/ColliderComponent.h"
#include "Application/Components/Physics/Movement/FollowCameraComponent.h"

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

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// MapEditorが保存したエンティティ1件からGameObjectを組み立てる。
//	コンポーネントの種類ごとの分岐は一切持たず、ComponentRegistryに登録された
//	create関数を呼ぶだけ。新しいコンポーネント種類を追加してもこの関数は無改修でよい。
//	回転はMapEntity側で既にQuaternionなので、Euler変換は一切不要になった
//	(以前あったCreateFromYawPitchRoll経由の変換と、それに伴う合成順序の不一致リスクは消えた)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
GameObject* TerrainFactory::CreateFromData(ObjectManager& objectManager, const MapEntity& data)
{
	auto* obj = objectManager.Instantiate(data.name);
	auto* transform = obj->AddComponent<TransformComponent>();

	transform->SetPosition(data.pos);
	transform->SetRotation(data.rotation);
	transform->SetScale(data.scale);

	// TODO: GameObject自体はMapEntity::idを知らない。将来「特定の配置オブジェクトを
	// スクリプト/トリガーから参照したい」需要が出てきたら、data.idを持たせる
	// 専用コンポーネント(例: MapEntityIdComponent)を追加するか、GameObjectFactory<int>
	// (registry_)経由の登録を検討すること

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