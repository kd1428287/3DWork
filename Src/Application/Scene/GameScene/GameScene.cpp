#include "GameScene.h"
#include"../SceneManager.h"

// system
#include "../../Systems/Input/InputSystem.h"

// factory
#include "../../Factories/Game/PlayerFactory.h"
#include "../../Factories/Common/CameraFactory.h"

// component
#include "../../Components/Render/ModelRenderComponent.h"

// プレイヤー
#include "../../Components/Player/PlayerInputComponent.h"
#include "../../Components/Movement/IMovementSource.h"
#include "../../Components/Movement/MovementComponent.h"

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
	/*if (GetAsyncKeyState('T') & 0x8000)
	{
		SceneManager::Instance().SetNextScene
		(
			SceneManager::SceneType::Title
		);
	}*/
}

void GameScene::Init()
{
	BaseScene::Init();

	auto* ground = objManager_->Instantiate("Ground");
	auto* groundTrans = ground->AddComponent<TransformComponent>();
	auto* groundModel = ground->AddComponent<ModelRenderComponent>(
		"Asset/Models/Terrains/Ground/Terrain.gltf"
	);
	groundTrans->SetPosition({ 0.f,0.f,0.f });
	
	playerFactory_ = std::make_unique<PlayerFactory>();
	auto* player = playerFactory_->CreatePlayer(*objManager_);
	
	cameraFactory_ = std::make_unique<CameraFactory>();
	auto* camera = cameraFactory_->CreateCamera(*objManager_, player->GetComponent<CameraTargetComponent>());

	// system
	inputSystem_ = std::make_unique<InputSystem>();
	inputSystem_->RegisterPlayer(player->GetComponent<PlayerInputComponent>());
	inputSystem_->RegisterCameraOrbit(camera->GetComponent<CameraOrbitComponent>());
}
