#include "GameScene.h"
#include"../SceneManager.h"

// system
#include "../../Systems/Input/InputSystem.h"

// factory
#include "../../Factories/Game/PlayerFactory.h"

// component
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

GameScene::GameScene()
{
	Init();
}

GameScene::~GameScene() = default;

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
}

void GameScene::Init()
{
	BaseScene::Init();

	//for (int i = 0; i < 9; i++)
	//{
	auto* ground = objManager_->Instantiate("Ground");
	auto* groundTrans = ground->AddComponent<TransformComponent>();
	auto* groundModel = ground->AddComponent<ModelRenderComponent>(
		"Asset/Models/Terrains/Ground/Terrain.gltf"
	);
	groundTrans->position = { 0.f,0.f,0.f };
	//}

	/*auto* player = objManager_->Instantiate("Player");
	player->AddComponent<TransformComponent>();
	auto* playerTarget = player->AddComponent<CameraTargetComponent>();
	auto* input = player->AddComponent<PlayerInputComponent>();
	auto* move = player->AddComponent<MovementComponent>(2.0f);
	move->SetMovementSource(input);*/
	
	playerFactory_ = std::make_unique<PlayerFactory>();
	auto* player = playerFactory_->CreatePlayer(*objManager_);
	

	// --- カメラ: CameraComponent(カメラであること) + CameraFollowComponent(追従) ---
	GameObject* camera = objManager_->Instantiate("MainCamera");
	camera->AddComponent<TransformComponent>();
	auto* camComp = camera->AddComponent<CameraComponent>();
	auto* follow = camera->AddComponent<CameraFollowComponent>();
	camera->AddComponent<CameraViewComponent>();
	follow->SetTarget(player->GetComponent<CameraTargetComponent>());
	follow->SetLocalOffset({ 0.0f, 1.0f, -5.0f });

	

	// system
	inputSystem_ = std::make_unique<InputSystem>();
	inputSystem_->RegisterPlayer(player->GetComponent<PlayerInputComponent>());
}
