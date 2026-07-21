#include "GameScene.h"
#include"../SceneManager.h"

// system
#include "../../Systems/InputSystem.h"
#include "../../Systems/Collision/ColliderRegistry.h"
#include "../../Systems/Collision/CollisionSystem.h"

// factory
#include "../../Factories/Game/PlayerFactory.h"
#include "../../Factories/Game/TerrainFactory.h"
#include "../../Factories/Game/EnemyFactory.h"
#include "../../Factories/Game/EnemyDefinition.h"
#include "../../Factories/Common/CameraFactory.h"

// component

#include "../../Components/Camera/CameraTargetComponent.h"

GameScene::GameScene()
{
	Init();
}

GameScene::~GameScene() = default;

void GameScene::OnUpdate(float deltaTime)
{
	// systemManager_の実行順にまとめたので、ここにはそこに乗らない
	// GameScene固有の処理(例: クリア判定など)だけを書く
}

void GameScene::Init()
{
	BaseScene::Init();

	// factory
	terrainFactory_ = std::make_unique<TerrainFactory>();
	auto* terrain = terrainFactory_->CreateTerrain(*objManager_);

	std::unordered_map<std::string, EnemyDefinition> map;
	map.emplace("enemy", EnemyDefinition{});
	enemyFactory_ = std::make_unique<EnemyFactory>(map);
	enemyFactory_->CreateEnemy(*objManager_, "enemy", Math::Vector3(0, 0, 5));

	playerFactory_ = std::make_unique<PlayerFactory>();
	auto* player = playerFactory_->CreatePlayer(*objManager_);

	cameraFactory_ = std::make_unique<CameraFactory>();
	//player->GetComponent<CameraTargetComponent>()->GetGeneration();
	auto* camera = cameraFactory_->CreateCamera(*objManager_, player->GetComponent<CameraTargetComponent>());

	

	// system
	inputSystem_ = std::make_unique<InputSystem>();
	inputSystem_->RegisterPlayer(player->GetComponent<PlayerInputComponent>());
	inputSystem_->RegisterCameraOrbit(camera->GetComponent<CameraOrbitComponent>());

	colliderRegistry_ = std::make_unique<ColliderRegistry>();
	collisionSystem_ = std::make_unique<CollisionSystem>();

	// 旧PreUpdate/PostUpdateで表現しようとしていた順序を、ここで明示的に登録する
	// 入力 → コライダー情報の更新 → 当たり判定、の順で毎フレーム実行される
	systemManager_->SetExecutionOrder(
		[this](float dt) { inputSystem_->Update(dt); },
		[this](float dt) { objManager_->PreUpdate(dt); },
		[this](float dt) { objManager_->Update(dt); },
		[this](float dt) { colliderRegistry_->Refresh(*objManager_); },
		[this](float dt) { collisionSystem_->Update(*colliderRegistry_); },
		[this](float dt) { objManager_->PostUpdate(dt); },
		[this](float dt) { objManager_->Flush(); }
	);
}