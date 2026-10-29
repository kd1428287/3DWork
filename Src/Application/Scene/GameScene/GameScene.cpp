#include "GameScene.h"
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

// factory
#include "Application/Factories/PrefabFactory.h"
#include "Application/Factories/GamePlay/Map/TerrainFactory.h"
#include "Application/Factories/Common/CameraFactory.h"

// definitions
#include "Application/Definitions/Loaders/MapLoader.h"

// result
#include "Application/Systems/GamePlay/ResultController.h"
#include "Application/Systems/GamePlay/ResultView.h"

// event
#include "Application/Core/EventBus/Events/CombatEvents.h"
#include "Application/Core/EventBus/Events/SceneEvents.h"
#include "Application/Core/EventBus/Events/TimeScaleEvents.h"
#include "Application/Core/EventBus/Events/InputLockEvents.h"

namespace
{
	constexpr const char* kDefaultPlayerPrefab = "Asset/Data/Game/Player.json";

	// BGM関連
	constexpr const char* kGameBgm = "Asset/Sounds/Game/Magatukami.wav";
	constexpr float kBgmFadeIn = 2.0f;
	constexpr float kBgmFadeOut = 1.0f;

	// リザルト・水墨画演出関連
	constexpr int kResultFontNo = 0;                // 衡山毛筆のKdFontManager登録番号
	constexpr float kResultTimeScale = 0.1f;        // 撃破後のスロー倍率
	const Math::Vector2 kInkCenter = { 0.5f, 0.5f }; // 墨が広がる起点(画面中央)

	// マーカーの位置・向きでPrefabを生成する。読み込みに失敗したらnullptr
	GameObject* SpawnAtMarker(ObjectManager& om, const std::string& path, const SpawnMarkerDef& m)
	{
		PrefabDefinition def;
		if (!PrefabFactory::LoadFromFile(path, def)) return nullptr;
		def.pos = m.position;
		def.rotation = m.rotation;
		return PrefabFactory::Create(om, def);
	}
}

GameScene::GameScene() = default;

GameScene::~GameScene() = default;

void GameScene::OnUpdate(float deltaTime)
{
#ifdef _DEBUG
	// 開発用: F7で撃破扱いにして演出を確認する
	if (GetAsyncKeyState(VK_F7) & 0x8000) { result_->Start(); }
#endif

	result_->Update(deltaTime, PollConfirm());

	// リザルト進行度に応じた水墨画風ポストプロセスの更新
	KdShaderManager::Instance().m_postProcessShader.SetInkWash(result_->GetInkProgress(), kInkCenter, result_->GetTime());
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

void GameScene::OnDrawSprite()
{
	// リザルト演出用UIの描画
	resultView_->Draw(*result_);
}

void GameScene::OnEnter()
{
	SetupResult();
	BuildWorld();
	BuildSystems();
	SetupEnvironment();
	Application::Instance().ShowCursolEnable(false);

	// 明転(In)に合わせてBGMをフェードイン再生
	bgm_ = Events::PublishSoundPlayEvent(*GLOBALEVENT, kGameBgm, true, 1.0f, 0.0f, kBgmFadeIn);
}

void GameScene::OnExit()
{
	// BGMのフェードアウト
	Events::PublishSoundFadeOutEvent(*GLOBALEVENT, bgm_, kBgmFadeOut);

	// ポストプロセスは共有なので、次のシーンへ水墨画化を持ち越さない
	KdShaderManager::Instance().m_postProcessShader.SetInkWash(0.0f, kInkCenter, 0.0f);

	KdInputManager::Instance().SetAxisConfineToWindowCenter("Look", false);
	Application::Instance().ShowCursolEnable(true);
}

void GameScene::BuildWorld()
{
	// 地形・マップ(スポーンマーカーはここで回収し、生成は後段で行う)
	TerrainFactory terrainFactory;
	terrainFactory.CreateSkydome(*objManager_, 0);

	std::vector<SpawnMarkerDef> markers;
	MapLoader loader;
	loader.LoadMapFromJson("Asset/Data/Map/MapData.json", *objManager_, terrainFactory, &markers);

	// 以降のオブジェクト生成より先にバスを購読させる(元の生成順を維持)
	slashTrailDispatcher_ = std::make_unique<SlashTrailDispatcher>();
	slashTrailDispatcher_->Init(*localBus_);
	SlashTrailParams params;
	slashTrailDispatcher_->RegisterDefinition("Sword", params);

	// 敵
	for (const auto& m : markers)
	{
		if (m.role == SpawnRole::None)
		{
			std::cerr << "[Spawn] marker " << m.id << " has no valid role" << std::endl;
			continue;
		}
		if (m.role != SpawnRole::Enemy) continue;

		GameObject* enemy = m.prefab.empty() ? nullptr : SpawnAtMarker(*objManager_, m.prefab, m);
		if (!enemy)
		{
			std::cerr << "[Spawn] enemy marker " << m.id << " failed: " << m.prefab << std::endl;
			continue;
		}
		// 敵の生存・撃破を監視対象に登録
		WatchEnemy(*enemy);
	}

	// プレイヤー(最初のPlayerマーカーを採用)
	GameObject* player = nullptr;
	for (const auto& m : markers)
	{
		if (m.role != SpawnRole::Player) continue;
		if (player)
		{
			std::cerr << "[Spawn] extra Player marker ignored: " << m.id << std::endl;
			continue;
		}
		player = SpawnAtMarker(*objManager_, m.prefab.empty() ? kDefaultPlayerPrefab : m.prefab, m);
	}

	// マーカーが無い古いマップ用のフォールバック(Prefab既定位置)
	if (!player)
	{
		std::cerr << "[Spawn] no Player marker. using prefab default position" << std::endl;
		PrefabDefinition def;
		if (PrefabFactory::LoadFromFile(kDefaultPlayerPrefab, def))
		{
			player = PrefabFactory::Create(*objManager_, def);
		}
	}

	// カメラ
	CameraFactory cameraFactory;
	cameraFactory.CreateCamera(*objManager_, player);

	// UI
	LoadUI("Asset/Data/UI/Hud.json");
}

void GameScene::BuildSystems()
{
	KdInputManager::Instance().SetAxisConfineToWindowCenter("Look", true);

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
	ambient.SetDirLight(Math::Vector3(-0.5f, -1.0f, -0.5f), Math::Vector3(1.0f, 1.0f, 1.0f));
	ambient.SetDirLightShadowArea(Math::Vector2(100.f, 100.f), 100);
	//ambient.AddPointLight(Math::Vector3(0.2f,0.2f,0.2f), 10.0f, Math::Vector3(0, 5, 0), true);
	ambient.SetFogEnable(false, true);
	ambient.SetheightFog({ 0.9f, 0.9f, 0.9f }, 80.f, -10.f, 100.f);
	ambient.SetAmbientLight(Math::Vector4(1.0f, 1.0f, 1.0f, 0.2f));

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

void GameScene::SetupResult()
{
	result_ = std::make_unique<ResultController>();
	resultView_ = std::make_unique<ResultView>();
	resultView_->SetFontNo(kResultFontNo);

	ResultController::Config cfg;
	cfg.lineCount = ResultView::kLineCount;
	result_->SetConfig(cfg);

	result_->onPhaseChanged = [this](ResultController::Phase phase)
		{
			switch (phase)
			{
			case ResultController::Phase::Slow:
				// 撃破の瞬間にスローへ。文字テクスチャもスローの間に作っておく
				Events::PublishHitStop(*localBus_, kResultTimeScale, -1.0f);
				{
					// 操作・視点をロック。コンポーネントはシーンと共に破棄されるので解除は不要
					Events::InputLockChanged lock;
					lock.locked = true;
					GLOBALEVENT->Publish(lock);
				}
				resultView_->Build(result_->GetPlayTime());
				break;
			case ResultController::Phase::Exit:
				GLOBALEVENT->Publish(Events::SceneChangeRequest(SceneType::Title, true));
				break;
			default:
				break;
			}
		};
}

void GameScene::WatchEnemy(GameObject& enemy)
{
	// 敵ごとのバスを購読する(誰が死んだかをHandleで比べずに済む)
	++aliveEnemies_;
	enemy.GetLocalEventBus().Subscribe<Events::Died>([this](const Events::Died&) { OnEnemyDied(); });
}

void GameScene::OnEnemyDied()
{
	// 配置した敵を全滅させたらボス撃破とみなす
	if (aliveEnemies_ > 0 && --aliveEnemies_ == 0)
	{
		result_->Start();
	}
}

bool GameScene::PollConfirm()
{
	// 押した瞬間だけtrue。入力アクションを登録したらIsPress("名前")へ差し替える
	const bool now = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
	const bool pressed = now && !prevConfirm_;
	prevConfirm_ = now;
	return pressed;
}