#include "TutorialScene.h"
#include <algorithm>
#include "Application/main.h"
#include "Application/Core/Scene/SceneManager.h"

// system
#include "Application/Systems/InputSystem.h"
#include "Application/Systems/GamePlay/TimeScaleSystem.h"
#include "Application/Systems/GamePlay/CameraSystem.h"
#include "Application/Systems/Collision/ColliderRegistry.h"
#include "Application/Systems/Collision/CollisionSystem.h"
#include "Application/Effect/Particle/EffectDispatcher.h"
#include "Application/Effect/SlashTrail/SlashTrailDispatcher.h"
#include "Application/Effect/Shape/ShapeEffectDispatcher.h"
#include "Application/Effect/Shell/WindShellDispatcher.h"

// factory
#include "Application/Factories/PrefabFactory.h"
#include "Application/Factories/GamePlay/Map/TerrainFactory.h"
#include "Application/Factories/Common/CameraFactory.h"

// definitions
#include "Application/Definitions/Loaders/MapLoader.h"

// tutorial
#include "Application/Systems/GamePlay/Tutorial/TutorialManager.h"

// event
#include "Application/Core/EventBus/Events/SceneEvents.h"

namespace
{
	constexpr const char* kDefaultPlayerPrefab = "Asset/Data/Game/Character/Player.json";
	constexpr const char* kMapData = "Asset/Data/Map/MapData.json";	// TODO: チュートリアル用マップに差し替える
	constexpr const char* kTutorialBgm = "Asset/Sounds/Game/Magatukami.wav";
	constexpr float kBgmFadeIn = 2.0f;
	constexpr float kBgmFadeOut = 1.0f;

	constexpr int kSkipKey = VK_RETURN;

	// 地形・マップを生成し、スポーンマーカーを返す
	std::vector<SpawnMarkerDef> LoadMap(ObjectManager& om)
	{
		TerrainFactory terrainFactory;
		terrainFactory.CreateSkydome(om, 0);

		std::vector<SpawnMarkerDef> markers;
		MapLoader loader;
		loader.LoadMapFromJson(kMapData, om, terrainFactory, &markers);
		return markers;
	}

	// Prefabを生成する。markerがあれば位置・向きを上書きし、読み込み失敗時はnullptr
	GameObject* SpawnPrefab(ObjectManager& om, const std::string& path, const SpawnMarkerDef* marker = nullptr)
	{
		PrefabDefinition def;
		if (!PrefabFactory::LoadFromFile(path, def)) return nullptr;
		if (marker)
		{
			def.pos = marker->position;
			def.rotation = marker->rotation;
		}
		return PrefabFactory::Create(om, def);
	}

	// 最初のPlayerマーカーに生成し、できなければPrefab既定位置に生成する
	GameObject* SpawnPlayer(ObjectManager& om, const std::vector<SpawnMarkerDef>& markers)
	{
		const auto it = std::find_if(markers.begin(), markers.end(),
			[](const SpawnMarkerDef& m) { return m.role == SpawnRole::Player; });

		if (it != markers.end())
		{
			const std::string path = it->prefab.empty() ? kDefaultPlayerPrefab : it->prefab;
			if (GameObject* player = SpawnPrefab(om, path, &*it)) return player;
		}

		std::cerr << "[Spawn] player marker unavailable. using prefab default position" << std::endl;
		return SpawnPrefab(om, kDefaultPlayerPrefab);
	}
}

TutorialScene::TutorialScene() = default;

TutorialScene::~TutorialScene() = default;

void TutorialScene::OnEnter()
{
	BuildWorld();
	BuildSystems();
	SetupEnvironment();
	SetupTutorial();
	Application::Instance().ShowCursolEnable(false);

	// 明転(In)に合わせてBGMをフェードイン再生
	bgm_ = Events::PublishSoundPlayEvent(*GLOBALEVENT, kTutorialBgm, true, 1.0f, 0.0f, kBgmFadeIn);
}

void TutorialScene::OnExit()
{
	// Stateが参照する基盤やシステムが生きているうちに終了させる
	if (tutorialManager_) { tutorialManager_->Finalize(); }

	Events::PublishSoundFadeOutEvent(*GLOBALEVENT, bgm_, kBgmFadeOut);

	KdInputManager::Instance().SetAxisConfineToWindowCenter("Look", false);
	Application::Instance().ShowCursolEnable(true);
}

void TutorialScene::OnUpdate(float deltaTime)
{
	if (leaving_) return;

	if (PollSkip()) { tutorialManager_->Skip(); }

	tutorialManager_->Update(deltaTime);

	// 完了・スキップのどちらでもボス戦へ遷移する
	if (tutorialManager_->IsFinished())
	{
		leaving_ = true;
		GLOBALEVENT->Publish(Events::SceneChangeRequest(SceneType::Game, true));
	}
}

void TutorialScene::OnPreDraw(float deltaTime)
{
	effectDispatcher_->Update(deltaTime);
	shapeEffectDispatcher_->Update(deltaTime);
	windShellDispatcher_->Update(deltaTime);
}

void TutorialScene::OnDrawEffects()
{
	effectDispatcher_->Draw(ParticleDrawPass::Default);
	slashTrailDispatcher_->Draw(ParticleDrawPass::Default);
	shapeEffectDispatcher_->Draw(ParticleDrawPass::Default);
	windShellDispatcher_->Draw(ParticleDrawPass::Default);
	effectDispatcher_->DrawLiquid();
}

void TutorialScene::OnDrawBright()
{
	effectDispatcher_->Draw(ParticleDrawPass::Bright);
	slashTrailDispatcher_->Draw(ParticleDrawPass::Bright);
	shapeEffectDispatcher_->Draw(ParticleDrawPass::Bright);
}

void TutorialScene::BuildWorld()
{
	// 敵はチュートリアル側で生成するので、ここではプレイヤーのみ
	const auto markers = LoadMap(*objManager_);

	// プレイヤーがバスを購読できるよう、生成より先に準備する
	SetupSlashTrail();
	GameObject* player = SpawnPlayer(*objManager_, markers);

	CameraFactory cameraFactory;
	cameraFactory.CreateCamera(*objManager_, player);

	LoadUI("Asset/Data/UI/Hud.json");
}

void TutorialScene::SetupSlashTrail()
{
}

void TutorialScene::BuildSystems()
{
	KdInputManager::Instance().SetAxisConfineToWindowCenter("Look", true);

	cameraSystem_ = std::make_unique<CameraSystem>(*objManager_);

	inputSystem_ = std::make_unique<InputSystem>(*localBus_);

	colliderRegistry_ = std::make_unique<ColliderRegistry>();
	collisionSystem_ = std::make_unique<CollisionSystem>();

	effectDispatcher_ = std::make_unique<EffectDispatcher>();
	effectDispatcher_->Init(*localBus_);

	shapeEffectDispatcher_ = std::make_unique<ShapeEffectDispatcher>();
	shapeEffectDispatcher_->Init(*localBus_);

	slashTrailDispatcher_ = std::make_unique<SlashTrailDispatcher>();
	slashTrailDispatcher_->Init();

	windShellDispatcher_ = std::make_unique<WindShellDispatcher>();
	windShellDispatcher_->Init(*localBus_);
	windShellDispatcher_->RegisterPreset("EnemyAura", WindShellPreset::EnemyAura());
	windShellDispatcher_->RegisterPreset("Charge", WindShellPreset::Charge());

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

void TutorialScene::SetupEnvironment()
{
	auto& post = KdShaderManager::Instance().m_postProcessShader;
	auto& ambient = KdShaderManager::Instance().WorkAmbientController();

	// ライト・フォグ
	ambient.SetDirLight(Math::Vector3(-0.45f, -0.75f, 0.48f), Math::Vector3(1.62f, 1.66f, 1.6f));
	ambient.SetDirLightShadowArea(Math::Vector2(100.f, 100.f), 100);
	ambient.SetFogEnable(false, true);
	ambient.SetheightFog({ 0.7f, 0.7f, 0.7f }, 80.f, -10.f, 100.f);
	ambient.SetAmbientLight(Math::Vector4(0.28f, 0.31f, 0.36f, 1.0f));

	// ポストプロセス
	post.SetFarClippingDistance(50.f);
	post.SetFocusRange(0, 50.0f);
	post.SetExposure(1.05f);
	post.SetContrast(1.25f);
	post.SetSaturation(0.85f);
	post.SetTemperature(-0.3f);
	post.SetTint(-0.15f);

	post.SetMotionBlurEnabled(true);
	post.SetMotionBlurStrength(0.f);
	post.SetMotionBlurMaxLength(0.08f);
	post.SetMotionBlurObjectStrength(1.0f);

	// 画面全体に重ねる質感テクスチャ(約0.15秒ごとに位置を切り替える)
	post.SetSurfaceTexture(KdAssets::Instance().m_textures.GetData("Asset/Textures/Game/p0028_l.png"));
	post.SetSurfaceIntensity(0.6f);
	post.SetSurfaceLumaRange(0.4f, 1.2f);
	post.SetSurfaceTransform({ 2.0f, 2.0f }, { 0, 0 });
	post.SetSurfaceJitter(0.15f);
}

void TutorialScene::SetupTutorial()
{
	TutorialContext context{};	// TODO: Stateが使う依存(ObjectManager、EventBus、プレイヤー等)を設定する
	TutorialConfig config{};	// TODO: 各フェーズの設定値(到達位置、敵の配置など)を設定する

	tutorialManager_ = std::make_unique<TutorialManager>();
	tutorialManager_->Initialize(context, config);
}

bool TutorialScene::PollSkip()
{
	// 非アクティブ時はキー入力を拾わない
	const bool now = GetForegroundWindow() == Application::Instance().GetWindowHandle()
		&& (GetAsyncKeyState(kSkipKey) & 0x8000) != 0;
	const bool pressed = now && !prevSkip_;
	prevSkip_ = now;
	return pressed;
}