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

// definitions
#include "Application/Definitions/Loaders/MapLoader.h"

// component
#include "Application/Components/GamePlay/Camera/CameraTargetComponent.h"
#include "Application/Components/Core/TransformComponent.h"
#include "Application/Components/Graphics/Animation/SkeletonComponent.h"
#include "Application/Components/Graphics/Animation/ModelAnimatorComponent.h"
#include "Application/Components/Graphics/Render/ModelRenderComponent.h"

GameScene::GameScene() = default;

GameScene::~GameScene() = default;

void GameScene::OnUpdate(float deltaTime)
{
	// systemManager_の実行順にまとめたので、ここにはそこに乗らない
	// GameScene固有の処理(例: クリア判定など)だけを書く

	auto tex = KdFontManager::Instance().CreateFontTexture(0, "こんにちは", 3);

	float x = 3, y = 3;
	for (auto& ch : tex->GetTexList())
	{
		KdShaderManager::Instance().m_spriteShader.DrawTex(ch->FontTex, x, y);
		x += ch->FontTex->GetInfo().Width;
	}
}

void GameScene::OnPreDraw(float deltaTime)
{
	effectDispatcher_->Update(deltaTime);
}

void GameScene::OnDrawEffects()
{
	effectDispatcher_->Draw(ParticleDrawPass::Default);
	slashTrailDispatcher_->Draw(ParticleDrawPass::Default);
	effectDispatcher_->DrawLiquid();
}

void GameScene::OnDrawBright()
{
	effectDispatcher_->Draw(ParticleDrawPass::Bright);
	slashTrailDispatcher_->Draw(ParticleDrawPass::Bright);
}

void GameScene::OnEnter()
{
	BuildWorld();
	BuildSystems();
	SetupEnvironment();
}

void GameScene::BuildWorld()
{
	// 地形・マップ
	TerrainFactory terrainFactory;
	terrainFactory.CreateSkydome(*objManager_, 0);

	MapLoader loader;
	loader.LoadMapFromJson("Asset/Data/Map/MapData.json", *objManager_, terrainFactory);

	// 以降のオブジェクト生成より先にバスを購読させる(元の生成順を維持)
	slashTrailDispatcher_ = std::make_unique<SlashTrailDispatcher>();
	slashTrailDispatcher_->Init(*localBus_);
	SlashTrailParams params;
	slashTrailDispatcher_->RegisterDefinition("Sword", params);

	// 敵
	std::unordered_map<std::string, std::string> enemyDefs;
	enemyDefs["Warrock"] = "Asset/Data/Game/Warrock.json";
	EnemyFactory enemyFactory(enemyDefs);

	if (enemyFactory.IsKnownEnemy("Warrock"))
	{
		enemyFactory.CreateEnemy(*objManager_, "Warrock", Math::Vector3(10, 0, 5.f));
	}

	// プレイヤーとカメラ
	PlayerFactory playerFactory;
	auto* player = playerFactory.CreatePlayer(*objManager_, "Asset/Data/Game/Player.json");

	CameraFactory cameraFactory;
	cameraFactory.CreateCamera(*objManager_, player);

	// UI
	LoadUI("Asset/Data/Game/Hud.json");
}

void GameScene::BuildSystems()
{
	cameraSystem_ = std::make_unique<CameraSystem>(*objManager_);

	inputSystem_ = std::make_unique<InputSystem>(*localBus_);

	colliderRegistry_ = std::make_unique<ColliderRegistry>();
	collisionSystem_ = std::make_unique<CollisionSystem>();

	effectDispatcher_ = std::make_unique<EffectDispatcher>();
	effectDispatcher_->Init(*localBus_);

	timeScaleSystem_ = std::make_unique<TimeScaleSystem>(*localBus_, *objManager_);

	// 更新順はここで一括管理する
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
}

void GameScene::SetupEnvironment()
{
	auto& post = KdShaderManager::Instance().m_postProcessShader;
	auto& ambient = KdShaderManager::Instance().WorkAmbientController();

	// ライト・フォグ
	ambient.SetDirLightShadowArea(Math::Vector2(100.f, 100.f), 100);
	ambient.AddPointLight(Math::Vector3(1.0f, 1.0f, 1.0f), 10.0f, Math::Vector3(0, 0, 0), false);
	ambient.SetFogEnable(false, true);
	ambient.SetheightFog({ 0.9f, 0.9f, 0.9f }, 80.f, -10.f, 100.f);
	ambient.SetAmbientLight(Math::Vector4(1.0f, 1.0f, 1.0f, 0.25f));

	// ポストプロセス(寒色寄り・低彩度・高コントラストで、鉄や血の冷たさと古びた和風の空気感を出す)
	post.SetFarClippingDistance(50.f);
	post.SetFocusRange(0, 50.0f);
	post.SetExposure(1.05f);
	post.SetContrast(1.25f);
	post.SetSaturation(0.85f);
	post.SetTemperature(-0.3f);
	post.SetTint(-0.15f);

	// 画面全体に重ねる質感テクスチャ(約0.15秒ごとに位置を切り替える)
	post.SetSurfaceTexture(KdAssets::Instance().m_textures.GetData("Asset/Textures/Game/p0028_l.png"));
	post.SetSurfaceIntensity(0.6f);
	post.SetSurfaceLumaRange(0.4f, 1.2f);
	post.SetSurfaceTransform({ 2.0f, 2.0f }, { 0, 0 });
	post.SetSurfaceJitter(0.15f);
}