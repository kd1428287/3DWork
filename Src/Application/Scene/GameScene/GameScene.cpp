#include "GameScene.h"
#include"../SceneManager.h"

// system
#include "../../Systems/InputSystem.h"
#include "../../Systems/TimeScaleSystem.h"
#include "../../Systems/Collision/ColliderRegistry.h"
#include "../../Systems/Collision/CollisionSystem.h"
#include "../../Effect/EffectDispatcher.h"

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
#include "../../Components/Transform/TransformComponent.h"
#include "../../Components/Animation/SkeletonComponent.h"
#include "../../Components/Animation/SkeletonComponent.h"
#include "../../Components/Animation/ModelAnimatorComponent.h"
#include "../../Components/Render/ModelRenderComponent.h"

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
	effectDispatcher_->Draw(ParticleDrawPass::Default);
}

void GameScene::OnDrawBlight()
{
	effectDispatcher_->Draw(ParticleDrawPass::Blight);
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
	def.modelScale = { 1.5f,1.5f,1.5f };
	def.modelScale = { 1.f,1.f,1.f };
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
	auto* camera = cameraFactory_->CreateCamera(*objManager_, player);

	/*auto* obj = objManager_->Instantiate("samurai");
	obj->AddComponent<TransformComponent>()->SetPosition({ 0,3.f,0 });
	auto* skl = obj->AddComponent<SkeletonComponent>();
	skl->SetModelData(KdAssets::Instance().m_modeldatas.GetData("Asset/Models/Character/GhostSamurai/GhostSamurai.gltf"));
	auto* anim = obj->AddComponent<ModelAnimatorComponent>();
	obj->AddComponent<ModelRenderComponent>();
	anim->Play("GhostSamurai_APose_Idle",true);*/

	// system
	inputSystem_ = std::make_unique<InputSystem>();
	inputSystem_->RegisterPlayer(player->GetComponent<PlayerInputComponent>());
	inputSystem_->RegisterCameraOrbit(camera->GetComponent<CameraOrbitComponent>());
	inputSystem_->RegisterObjectManager(objManager_.get());

	colliderRegistry_ = std::make_unique<ColliderRegistry>();
	collisionSystem_ = std::make_unique<CollisionSystem>();

	effectDispatcher_ = std::make_unique<EffectDispatcher>();
	effectDispatcher_->Init(*localBus_);

	timeScaleSystem_ = std::make_unique<TimeScaleSystem>(*localBus_, *objManager_);

	systemManager_->SetExecutionOrder(
		[this](float dt) { inputSystem_->Update(dt); },
		[this](float dt) { timeScaleSystem_->Update(dt); },
		[this](float dt) { objManager_->PreUpdate(dt); },
		[this](float dt) { objManager_->Update(dt); },
		[this](float dt) { colliderRegistry_->Refresh(*objManager_); },
		[this](float dt) { collisionSystem_->Update(*colliderRegistry_); },
		[this](float dt) { effectDispatcher_->Update(dt); },
		[this](float dt) { objManager_->PostUpdate(dt); },
		[this](float dt) { objManager_->Flush(); }
	);

	KdShaderManager::Instance().WorkAmbientController().SetFogEnable(false, true);
	KdShaderManager::Instance().WorkAmbientController().SetheightFog({0.9f,0.9f,0.9f}, 10.f, -10.f, 100.f);
	KdShaderManager::Instance().m_postProcessShader.SetExposure(1.05f);
	KdShaderManager::Instance().m_postProcessShader.SetContrast(1.25f);       // コントラスト強め
	KdShaderManager::Instance().m_postProcessShader.SetSaturation(0.90f);     // 彩度低め
	KdShaderManager::Instance().m_postProcessShader.SetTemperature(-0.15f);   // ★わずかに寒色（青み）を寄せて鉄や血の冷たさを演出
	KdShaderManager::Instance().m_postProcessShader.SetTint(-0.05f);          // ★ごくわずかに緑に寄せて、古びた日本的・和風の空気感を作る;;;
	
}