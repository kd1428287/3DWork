#include "PlayerFactory.h"

#include "../../Components/Transform/TransformComponent.h"
#include "../../Components/Transform/SocketComponent.h"
#include "../../Components/Transform/AttachToSocketComponent.h"
#include "../../Components/Movement/MovementComponent.h"
#include "../../Components/Movement/VelocityComponent.h"
#include "../../Components/Movement/FacingDirectionComponent.h"
#include "../../Components/Character/Player/PlayerInputComponent.h"
#include "../../Components/Camera/CameraTargetComponent.h"
#include "../../Components/Render/ModelRenderComponent.h"
#include "../../Components/Animation/ModelAnimatorComponent.h"
#include "../../Components/Animation/SkeletonComponent.h"
#include "../../Components/Animation/BoneSocketComponent.h"
#include "../../Components/Character/Player/PlayerStatusController.h"
#include "../../Components/Collision/GravityComponent.h"
#include "../../Components/Collision/AttackSourceComponent.h"
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
	// GravityComponentをVelocityComponentより先にAddComponentしている。
	// 理由はEnemyFactory::BuildEnemy側のコメント参照
	// (着地時の速度クリアが、同じフレームのVelocityComponent::Updateによる
	//  位置反映より先に確定するようにするため。順序が逆だと着地の瞬間に
	//  地面へめり込む→押し戻される、を繰り返す不安定な挙動になる)。
	player->AddComponent<GravityComponent>();
	player->AddComponent<VelocityComponent>();
	auto* collider = player->AddComponent<ColliderComponent>();
	player->AddComponent<GroundSensorComponent>();
	player->AddComponent<FacingDirectionComponent>();

	// 3. 入力と移動の依存関係の注入
	auto* input = player->AddComponent<PlayerInputComponent>();
	auto* move = player->AddComponent<MovementComponent>(2.0f);
	move->SetMovementSource(input); // 参照関係の構築

	player->AddComponent<CameraTargetComponent>();
	// Y方向の半径をGroundSensorComponentのデフォルトfootOffset(0.9f)と
	// 一致させている。以前は0.5fだったため、Bodyの下端(-0.5)が
	// footOffset(0.9)まで届いておらず、Bodyが地面に乗って静止した高さと
	// GroundSensorComponentの接地レイの基準高さがズレていた。詳細は
	// EnemyFactory::BuildEnemy側の同種の修正コメントを参照
	// (このズレが「地面にめり込んだまま接地判定されず、重力が
	//  際限なく積み上がって最終的に地面を貫通する」不具合の根本原因だった)。
	collider->AddBox("Body", Math::Vector3(0.3f, 0.9f, 0.25f), {}, ColliderCategory::Bump);
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

	CreateWeapon(objectManager, player, rhandle);

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

GameObject* PlayerFactory::CreateWeapon(ObjectManager& objectManager, GameObject* player, Handle<TransformComponent>& handle)
{
	GameObject* weapon = objectManager.Instantiate("weapon");
	if (!weapon) return nullptr;
	auto* local = weapon->AddComponent<TransformComponent>();
	/*Math::Matrix mat = DirectX::XMMatrixLookAtLH({ 0,0,1 }, { 0,0,0 }, { 0,1,0 });
	local->SetRotation(Math::Quaternion::CreateFromRotationMatrix(XMMatrixTranspose(mat)));*/
	local->SetScale({ 0.5f,0.5f,0.5f, });
	weapon->AddComponent<AttachToSocketComponent>(handle);
	auto* skeleton = weapon->AddComponent<SkeletonComponent>();
	skeleton->SetModelData("Asset/Models/Character/Player/box.gltf");
	weapon->AddComponent<ModelRenderComponent>();
	auto* collision = weapon->AddComponent<ColliderComponent>();
	collision->AddBox("HitBox", Math::Vector3(1.f, 1.f, 1.f), Math::Vector3(0.f, 0.f, 0.f), ColliderCategory::HitBox);
	collision->IgnoreCollisionWith(player);
	weapon->AddComponent<AttackSourceComponent>();

	return weapon;
}