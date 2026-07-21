#include "PlayerFactory.h"

#include "../../Components/Transform/TransformComponent.h"
#include "../../Components/Transform/SocketComponent.h"
#include "../../Components/Transform/AttachToSocketComponent.h"
#include "../../Components/Movement/MovementComponent.h"
#include "../../Components/Movement/VelocityComponent.h"
#include "../../Components/Character/Player/PlayerInputComponent.h"
#include "../../Components/Camera/CameraTargetComponent.h"
#include "../../Components/Render/ModelRenderComponent.h"
#include "../../Components/Render/ModelAnimatorComponent.h"
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
	//trans->SetScale({ 0.01f,0.01f,0.01f });
	player->AddComponent<ModelRenderComponent>(
		//"Asset/Models/Character/Player/box.gltf"
		"Asset/Models/Character/Player/Walking.gltf"
	);
	player->AddComponent<ModelAnimatorComponent>()->SetFPS(60);
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
	/*std::vector<Math::Vector3> triangles;
	std::shared_ptr<TriangleMeshData> tMeshData = std::make_shared<TriangleMeshData>();
	for (auto& index : model->GetCollisionMeshNodeIndices())
	{
		tMeshData->C
		triangles = model->GetMesh(index)->GetVertexPositions();
	}
	collider->AddMesh("body", tMeshData, ColliderLayer::HurtBox);*/

	// ソケットの生成
	
	Handle<TransformComponent> handle(trans);
	auto* LShoulder = CreateSocket(objectManager, "LShoulder", handle);
	handle = Handle<TransformComponent>(LShoulder->GetComponent<SocketComponent>());
	auto* LElbow = CreateSocket(objectManager, "LElbow", handle);
	handle = Handle<TransformComponent>(LElbow->GetComponent<SocketComponent>());
	auto* LHand = CreateSocket(objectManager, "LHand", handle);
	handle = Handle<TransformComponent>(trans);
	auto* RShoulder = CreateSocket(objectManager, "RShoulder", handle);
	handle = Handle<TransformComponent>(RShoulder->GetComponent<SocketComponent>());
	auto* RElbow = CreateSocket(objectManager, "RElbow", handle);
	handle = Handle<TransformComponent>(RElbow->GetComponent<SocketComponent>());
	auto* RHand = CreateSocket(objectManager, "RHand", handle);
	handle = Handle<TransformComponent>(RHand->GetComponent<SocketComponent>());

	CreateWeapon(objectManager, handle);
	

	// 4. 所有権を持たない「利用権（参照用）」としての生ポインタを返す
	return player;
}

GameObject* PlayerFactory::CreateSocket(ObjectManager& objectManager, std::string objID, Handle<TransformComponent>& handle)
{
	auto* obj = objectManager.Instantiate(objID);
	if (!obj) return nullptr;
	auto* local = obj->AddComponent<SocketComponent>(handle);
	local->SetPosition({ 0,0,1 });
	return obj;
}

GameObject* PlayerFactory::CreateWeapon(ObjectManager& objectManager, Handle<TransformComponent>& handle)
{
	GameObject* weapon = objectManager.Instantiate("weapon");
	if (!weapon) return nullptr;
	auto* local = weapon->AddComponent<TransformComponent>();
	Math::Matrix mat = DirectX::XMMatrixLookAtLH({ 0,0,1 }, { 0,0,0 }, { 0,1,0 });
	local->SetRotation(Math::Quaternion::CreateFromRotationMatrix(XMMatrixTranspose(mat)));
	weapon->AddComponent<AttachToSocketComponent>(handle);
	//weapon->AddComponent<ColliderComponent>()->AddAABB("body", Math::Vector3(0.1f, 0.1f, 0.5f), {},ColliderLayer::Hitbox);
	weapon->AddComponent<ModelRenderComponent>(
		"Asset/Models/Character/Player/box.gltf"
	);

	return weapon;
}
