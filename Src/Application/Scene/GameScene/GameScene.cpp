#include "GameScene.h"
#include"../SceneManager.h"
#include "../../Factories/CardFactory.h"

#include <unordered_map>
#include "../../Components/Card/CardDefinition.h"

#include "../../Components/Render/ModelRenderComponent.h"

// プレイヤー
#include "../../Components/Player/PlayerInputComponent.h"
#include "../../Components//Transform/IMovementSource.h"
#include "../../Components/Transform/MovementComponent.h"

// カメラ
#include "../../Components/Camera/CameraComponent.h"
#include "../../Components/Camera/CameraFollowComponent.h"
#include "../../Components/Camera/CameraViewComponent.h"
#include "../../Components/Camera/CameraTargetComponent.h"

void GameScene::PreUpdate()
{
	BaseScene::PreUpdate();
	inputSystem_->Update(0);
}

void GameScene::Event()
{
	if (GetAsyncKeyState('T') & 0x8000)
	{
		SceneManager::Instance().SetNextScene
		(
			SceneManager::SceneType::Title
		);
	}

	cardSelectionSystem_->Update(0);
}

void GameScene::Init()
{
	BaseScene::Init();


	auto* ground = objManager_->Instantiate("Ground");
	auto* groundTrans = ground->AddComponent<TransformComponent>();
	auto* groundModel = ground->AddComponent<ModelRenderComponent>(
		"Asset/Models/Terrains/Ground/Terrain.gltf"
	);
	groundTrans->position = { 0,0.f,0.f };
	groundTrans->matrix =
		Math::Matrix::CreateScale(1.f) * 
		Math::Matrix::CreateTranslation(groundTrans->position);

	auto* player = objManager_->Instantiate("Player");
	player->AddComponent<TransformComponent>();
	auto* playerTarget = player->AddComponent<CameraTargetComponent>();
	auto* input = player->AddComponent<PlayerInputComponent>();
	auto* move = player->AddComponent<MovementComponent>(2.0f);
	move->SetMovementSource(input);

	// --- カメラ: CameraComponent(カメラであること) + CameraFollowComponent(追従) ---
	GameObject* camera = objManager_->Instantiate("MainCamera");
	camera->AddComponent<TransformComponent>();
	auto* camComp = camera->AddComponent<CameraComponent>();
	auto* follow = camera->AddComponent<CameraFollowComponent>();
	camera->AddComponent<CameraViewComponent>();
	follow->SetTarget(playerTarget);
	follow->SetLocalOffset({ 0.0f, 0.0f, -5.0f });

	// system
	inputSystem_ = std::make_unique<InputSystem>();
	inputSystem_->RegisterPlayer(input);
	handSystem_ = std::make_unique<HandSystem>();
	cardSelectionSystem_ = std::make_unique<CardSelectionSystem>(*handSystem_, *localBus_);

	CardDefinition def;
	std::unordered_map<std::string, CardDefinition> map;
	map.emplace("Attack", def);
	cardFactory_ = std::make_unique<CardFactory>(
		map
	);
	cardFactory_->CreateCard(*objManager_, "Attack", 0);
}
