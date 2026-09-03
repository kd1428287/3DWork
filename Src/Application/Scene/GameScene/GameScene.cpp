#include "GameScene.h"
#include"../SceneManager.h"

// system
#include "../../Systems/InputSystem.h"
#include "../../Systems/Collision/ColliderRegistry.h"
#include "../../Systems/Collision/CollisionSystem.h"
#include "../../DIspatcher/EffectDispatcher.h"

// factory
#include "../../Factories/Game/PlayerFactory.h"
#include "../../Factories/Game/PlayerDefinitionLoader.h"
#include "../../Factories/Map/TerrainFactory.h"
#include "../../Factories/Game/EnemyFactory.h"
#include "../../Components/Character/Enemy/EnemyDefinition.h"
#include "../../Components/Character/Enemy/Warrock/WarrockAIData.h"
#include "../../Components/Character/Enemy/Brute/BruteAIData.h"
#include "../../Components/Character/Enemy/EnemyDefinition.h"
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

void GameScene::OnDrawEffects()
{
	effectDispatcher_->Draw();
}

void GameScene::Init()
{
	BaseScene::Init();

	// factory
	terrainFactory_ = std::make_unique<TerrainFactory>();
	auto* terrain = terrainFactory_->CreateTerrain(*objManager_,0);

	std::unordered_map<std::string, EnemyDefinition> map;
	EnemyDefinition def;
	def.name = "Brute";
	def.aiData = CreateDebugBruteAIData();
	map.emplace("Brute", def);
	def = {};
	def.name = "Warrock";
	def.type = EnemyType::Warrock;
	def.modelPath = "Asset/Models/Character/Warrock/Warrock.gltf";
	def.modelScale = { 2.0f,2.0f,2.0f };
	def.aiData = CreateDebugWarrockAIData();
	map.emplace("Warrock", def);
	enemyFactory_ = std::make_unique<EnemyFactory>(map);
	//enemyFactory_->CreateEnemy(*objManager_, "Brute", Math::Vector3(0, 0, 5));
	enemyFactory_->CreateEnemy(*objManager_, "Warrock", Math::Vector3(10, 0, 5));

	playerFactory_ = std::make_unique<PlayerFactory>();
	PlayerDefinition pDef;
	PlayerDefinitionLoader::LoadFromFile("Asset/Data/Game/Player.json", pDef);
	auto* player = playerFactory_->CreatePlayer(*objManager_, pDef);

	cameraFactory_ = std::make_unique<CameraFactory>();
	//player->GetComponent<CameraTargetComponent>()->GetGeneration();
	auto* camera = cameraFactory_->CreateCamera(*objManager_, player);

	// system
	inputSystem_ = std::make_unique<InputSystem>();
	inputSystem_->RegisterPlayer(player->GetComponent<PlayerInputComponent>());
	inputSystem_->RegisterCameraOrbit(camera->GetComponent<CameraOrbitComponent>());
	inputSystem_->RegisterObjectManager(objManager_.get());

	colliderRegistry_ = std::make_unique<ColliderRegistry>();
	collisionSystem_ = std::make_unique<CollisionSystem>();

	effectDispatcher_ = std::make_unique<EffectDispatcher>();
	effectDispatcher_->Init(*localBus_);

	// 旧PreUpdate/PostUpdateで表現しようとしていた順序を、ここで明示的に登録する
	// 入力 → コライダー情報の更新 → 当たり判定、の順で毎フレーム実行される
	systemManager_->SetExecutionOrder(
		[this](float dt) { inputSystem_->Update(dt); },
		[this](float dt) { objManager_->PreUpdate(dt); },
		[this](float dt) { objManager_->Update(dt); },
		[this](float dt) { colliderRegistry_->Refresh(*objManager_); },
		[this](float dt) { collisionSystem_->Update(*colliderRegistry_); },
		[this](float dt) { effectDispatcher_->Update(dt); },
		[this](float dt) { objManager_->PostUpdate(dt); },
		[this](float dt) { objManager_->Flush(); }
	);
}