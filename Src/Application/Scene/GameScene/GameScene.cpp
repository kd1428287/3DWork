#include "GameScene.h"
#include"Application/Core/Scene/SceneManager.h"

// system
#include "Application/Systems/InputSystem.h"
#include "Application/Systems/GamePlay/TimeScaleSystem.h"
#include "Application/Systems/GamePlay/CameraSystem.h"
#include "Application/Systems/Collision/ColliderRegistry.h"
#include "Application/Systems/Collision/CollisionSystem.h"
#include "Application/Effect/Particle/EffectDispatcher.h"
#include "Application/Effect/SlashTrail/SlashTrailDispatcher.h"

// factory
#include "Application/Factories/GamePlay/Character/PlayerFactory.h"
#include "Application/Factories/GamePlay/Character/EnemyFactory.h"
#include "Application/Factories/GamePlay/Map/TerrainFactory.h"
#include "Application/Factories/Common/CameraFactory.h"
#include "Application/Factories/PrefabFactory.h"


// definitions
#include "Application/Definitions/Loaders/MapLoader.h"

// component
#include "Application/Components/GamePlay/Camera/CameraTargetComponent.h"
#include "Application/Components/Core/TransformComponent.h"
#include "Application/Components/Graphics/Animation/SkeletonComponent.h"
#include "Application/Components/Graphics/Animation/ModelAnimatorComponent.h"
#include "Application/Components/Graphics/Render/ModelRenderComponent.h"

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

void GameScene::OnPreDraw(float deltaTime)
{
	effectDispatcher_->Update(deltaTime);
}

void GameScene::OnDrawEffects()
{
	effectDispatcher_->Draw(ParticleDrawPass::Default);
	slashTrailDispatcher_->Draw(ParticleDrawPass::Default);
}

void GameScene::OnDrawBright()
{
	effectDispatcher_->Draw(ParticleDrawPass::Bright);
	slashTrailDispatcher_->Draw(ParticleDrawPass::Bright);
}


void GameScene::Init()
{
	BaseScene::Init();

	// factory
	terrainFactory_ = std::make_unique<TerrainFactory>();
	//auto* terrain = terrainFactory_->CreateTerrain(*objManager_, 0);
	auto* skydome = terrainFactory_->CreateSkydome(*objManager_, 0);
	auto loader = std::make_unique<MapLoader>();

	loader->LoadMapFromJson("Asset/Data/Map/MapData.json", *objManager_, *terrainFactory_);

	slashTrailDispatcher_ = std::make_unique<SlashTrailDispatcher>();
	slashTrailDispatcher_->Init(*localBus_);
	SlashTrailParams params;
	slashTrailDispatcher_->RegisterDefinition("Sword", params);

	std::unordered_map<std::string, std::string> map;
	map["Warrock"] = "Asset/Data/Game/Warrock.json";
	enemyFactory_ = std::make_unique<EnemyFactory>(map);

	if (enemyFactory_->IsKnownEnemy("Warrock"))
	{
		enemyFactory_->CreateEnemy(*objManager_, "Warrock", Math::Vector3(10, 0, 5.f));
	}

	playerFactory_ = std::make_unique<PlayerFactory>();
	auto* player = playerFactory_->CreatePlayer(*objManager_, "Asset/Data/Game/Player.json");

	cameraFactory_ = std::make_unique<CameraFactory>();
	auto* camera = cameraFactory_->CreateCamera(*objManager_, player);

	PrefabDefinition def;
	if (PrefabFactory::LoadFromFile("Asset/Data/Game/Hud.json", def)) {
		PrefabFactory::Create(*objManager_, def);
	}

	cameraSystem_ = std::make_unique<CameraSystem>(*objManager_);

	// system
	inputSystem_ = std::make_unique<InputSystem>(*localBus_);

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
		[this](float dt) { slashTrailDispatcher_->Update(dt); },
		[this](float dt) { objManager_->PostUpdate(dt); },
		[this](float dt) { objManager_->Flush(); }
	);

	KdShaderManager::Instance().m_postProcessShader.SetFarClippingDistance(50.f);
	KdShaderManager::Instance().m_postProcessShader.SetFocusRange(0, 50.0f);
	KdShaderManager::Instance().WorkAmbientController().SetDirLightShadowArea(Math::Vector2(100.f, 100.f), 100);
	KdShaderManager::Instance().WorkAmbientController().AddPointLight(Math::Vector3(1.0f, 1.0f, 1.0f), 10.0f, Math::Vector3(0, 0, 0), false);
	KdShaderManager::Instance().WorkAmbientController().SetFogEnable(false, true);
	KdShaderManager::Instance().WorkAmbientController().SetheightFog({0.9f,0.9f,0.9f}, 80.f, -10.f, 100.f);
	KdShaderManager::Instance().WorkAmbientController().SetAmbientLight(Math::Vector4(1.0f,1.0f,1.0f, 0.25f));
	KdShaderManager::Instance().m_postProcessShader.SetExposure(1.05f);
	KdShaderManager::Instance().m_postProcessShader.SetContrast(1.25f);       // コントラスト強め
	KdShaderManager::Instance().m_postProcessShader.SetSaturation(0.85f);     // 彩度低め
	KdShaderManager::Instance().m_postProcessShader.SetTemperature(-0.3f);   // ★わずかに寒色（青み）を寄せて鉄や血の冷たさを演出
	KdShaderManager::Instance().m_postProcessShader.SetTint(-0.15f);          // ★ごくわずかに緑に寄せて、古びた日本的・和風の空気感を作る
	
}