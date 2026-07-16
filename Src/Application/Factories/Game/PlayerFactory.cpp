#include "PlayerFactory.h"

#include "../../Components/Transform/TransformComponent.h"
#include "../../Components/Movement/MovementComponent.h"
#include "../../Components/Movement/VelocityComponent.h"
#include "../../Components/Player/PlayerInputComponent.h"
#include "../../Components/Camera/CameraTargetComponent.h"
#include "../../Components/Render/ModelRenderComponent.h"
#include "../../Components/Player/PlayerStatusController.h"
#include "../../Components/Collision/GravityComponent.h"
#include "../../Components/Sensors/GroundSensorComponent.h"

GameObject* PlayerFactory::CreatePlayer(ObjectManager& objectManager, int ownerPlayerId)
{
	// 1. ObjectManagerがInstantiate時に内部の std::vector<std::unique_ptr<GameObject>> 等に
	//    所有権を格納し、その生ポインタを返してくれている想定
	GameObject* player = objectManager.Instantiate("player");
	if (!player) return nullptr;

	// 2. コンポーネントをアタッチ
	auto* trans = player->AddComponent<TransformComponent>();
	trans->SetPosition({ 0.f, 0.f, 0.f });
	trans->SetScale({ 0.01f,0.01f,0.01f });
	player->AddComponent<ModelRenderComponent>(
		"Asset/Models/Character/Player/Walking.gltf"
		//"Asset/Models/Character/Player/box.gltf"
	);
	player->AddComponent<PlayerStatusController>();
	player->AddComponent<VelocityComponent>();
	player->AddComponent<ColliderComponent>()->AddAABB("body", Math::Vector3(0.4f, 0.9f, 0.4f), Math::Vector3::Zero, ColliderLayer::Bump);
	player->AddComponent<GravityComponent>();
	player->AddComponent<GroundSensorComponent>();

	// 3. 入力と移動の依存関係の注入
	auto* input = player->AddComponent<PlayerInputComponent>();
	auto* move = player->AddComponent<MovementComponent>(2.0f);
	move->SetMovementSource(input); // 参照関係の構築

	player->AddComponent<CameraTargetComponent>();

	// 4. 所有権を持たない「利用権（参照用）」としての生ポインタを返す
	return player;
}
