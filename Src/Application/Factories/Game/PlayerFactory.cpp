#include "PlayerFactory.h"

#include "../../Components/Transform/TransformComponent.h"
#include "../../Components/Transform/SocketComponent.h"
#include "../../Components/Transform/AttachToSocketComponent.h"
#include "../../Components/Movement/MovementComponent.h"
#include "../../Components/Movement/VelocityComponent.h"
#include "../../Components/Character/Player/PlayerInputComponent.h"
#include "../../Components/Camera/CameraTargetComponent.h"
#include "../../Components/Render/ModelRenderComponent.h"
#include "../../Components/Animation/ModelAnimatorComponent.h"
#include "../../Components/Animation/SkeletonComponent.h"
#include "../../Components/Animation/BoneSocketComponent.h"
#include "../../Components/Character/Player/PlayerStatusController.h"
#include "../../Components/Collision/GravityComponent.h"
#include "../../Components/Sensors/GroundSensorComponent.h"

GameObject* PlayerFactory::CreatePlayer(ObjectManager& objectManager)
{
	// 1. ObjectManagerがInstantiate時に内部の std::vector<std::unique_ptr<GameObject>> 等に
	//    所有権を格納し、その生ポインタを返してくれている想定
	GameObject* player = objectManager.Instantiate("player");
	if (!player) return nullptr;

	// 2. コンポーネントをアタッチ
	auto* trans = player->AddComponent<TransformComponent>();
	trans->SetPosition({ 0.f, 0.f, 0.f });
	player->AddComponent<ModelRenderComponent>();
	auto* skeleton = player->AddComponent<SkeletonComponent>();
	skeleton->SetModelData("Asset/Models/Character/Player/Walking.gltf");
	auto* animator = player->AddComponent<ModelAnimatorComponent>();
	animator->SetFPS(60);
	animator->Play("mixamo.com");
	player->AddComponent<PlayerStatusController>();
	player->AddComponent<VelocityComponent>();
	auto* collider = player->AddComponent<ColliderComponent>();
	player->AddComponent<GravityComponent>();
	player->AddComponent<GroundSensorComponent>();

	// 3. 入力と移動の依存関係の注入
	auto* input = player->AddComponent<PlayerInputComponent>();
	auto* move = player->AddComponent<MovementComponent>(2.0f);
	move->SetMovementSource(input); // 参照関係の構築

	player->AddComponent<CameraTargetComponent>();
	collider->AddBox("body", Math::Vector3(0.3f, 0.5f, 0.25f), {}, ColliderLayer::HurtBox);
	auto model = KdAssets::Instance().m_modeldatas.GetData("Asset/Models/Character/Player/box.gltf");

	// ソケットの生成
	
	Handle<SkeletonComponent> handle(skeleton);
	auto* LShoulder = CreateSocket(objectManager, "mixamorig:LeftShoulder", handle);
	auto* LElbow = CreateSocket(objectManager, "mixamorig:LeftArm", handle);
	auto* LHand = CreateSocket(objectManager, "mixamorig:LeftHand", handle);
	auto* RShoulder = CreateSocket(objectManager, "mixamorig:RightShoulder", handle);
	auto* RElbow = CreateSocket(objectManager, "mixamorig:RightArm", handle);
	auto* RHand = CreateSocket(objectManager, "mixamorig:RightHand", handle);

	Handle<TransformComponent> rhandle(RHand->GetComponent<BoneSocketComponent>());

	CreateWeapon(objectManager, rhandle);

	// 4. 所有権を持たない「利用権（参照用）」としての生ポインタを返す
	return player;
}

GameObject* PlayerFactory::CreateSocket(ObjectManager& objectManager, std::string objID, Handle<SkeletonComponent>& handle)
{
	auto* obj = objectManager.Instantiate(objID);
	if (!obj) return nullptr;
	auto* local = obj->AddComponent<BoneSocketComponent>(handle, objID);
	//local->SetPosition({ 0,0,1 });,
	return obj;
}

GameObject* PlayerFactory::CreateWeapon(ObjectManager& objectManager, Handle<TransformComponent>& handle)
{
	GameObject* weapon = objectManager.Instantiate("weapon");
	if (!weapon) return nullptr;
	auto* local = weapon->AddComponent<TransformComponent>();
	Math::Matrix mat = DirectX::XMMatrixLookAtLH({ 0,0,1 }, { 0,0,0 }, { 0,1,0 });
	local->SetRotation(Math::Quaternion::CreateFromRotationMatrix(XMMatrixTranspose(mat)));
	local->SetScale({ 0.5f,0.5f,0.5f, });
	weapon->AddComponent<AttachToSocketComponent>(handle);
	auto* skeleton = weapon->AddComponent<SkeletonComponent>();
	skeleton->SetModelData(
		KdAssets::Instance().m_modeldatas.GetData("Asset/Models/Character/Player/box.gltf")
	);
	weapon->AddComponent<ModelRenderComponent>();

	return weapon;
}
