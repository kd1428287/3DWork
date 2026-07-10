#include "PlayerFactory.h"

#include "../../Components/Transform/TransformComponent.h"
#include "../../Components/Transform/MovementComponent.h"
#include "../../Components/Player/PlayerInputComponent.h"
#include "../../Components/Camera/CameraTargetComponent.h"
#include "../../Components/Render/ModelRenderComponent.h"

GameObject* PlayerFactory::CreatePlayer(ObjectManager& objectManager, int ownerPlayerId)
{
	// 1. ObjectManagerがInstantiate時に内部の std::vector<std::unique_ptr<GameObject>> 等に
	//    所有権を格納し、その生ポインタを返してくれている想定
	GameObject* player = objectManager.Instantiate("Player");
	if (!player) return nullptr;

	// 2. コンポーネントをアタッチ
	player->AddComponent<TransformComponent>();
	player->AddComponent<ModelRenderComponent>(
		"Asset/Models/Character/Base/final low poly character  rigged.gltf"
	);

	// 3. 入力と移動の依存関係の注入
	auto* input = player->AddComponent<PlayerInputComponent>();
	auto* move = player->AddComponent<MovementComponent>();
	move->SetMovementSource(input); // 参照関係の構築

	player->AddComponent<CameraTargetComponent>();

	// 4. 所有権を持たない「利用権（参照用）」としての生ポインタを返す
	return player;
}
