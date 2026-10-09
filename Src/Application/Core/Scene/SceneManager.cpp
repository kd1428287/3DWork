#include "SceneManager.h"
#include "Application/main.h"

#include "Application/Scene/BaseScene/BaseScene.h"
#include "Application/Scene/TitleScene/TitleScene.h"
#include "Application/Scene/GameScene/GameScene.h"
#include "Application/Scene/ResultScene/ResultScene.h"
#include "Application/Core/EventBus/Events/SceneEvents.h"

SceneManager::SceneManager()
{
	Init();
}

SceneManager::~SceneManager()
{
	// Shutdown()済みなら何も起きない保険
	Shutdown();
}

void SceneManager::Init()
{
	factories_[SceneType::Title] = [] { return std::make_unique<TitleScene>(); };
	factories_[SceneType::Game] = [] { return std::make_unique<GameScene>(); };
	factories_[SceneType::Result] = [] { return std::make_unique<ResultScene>(); };

	// どちらのイベントも予約するだけ。実行中のシーンを即時に破棄しない
	sceneChangeSub_ = ScopedSubscriber(&GLOBALEVENT,
		GLOBALEVENT.Subscribe<Events::SceneChangeRequestEvent>(
			[this](const Events::SceneChangeRequestEvent& e)
			{
				SetNextScene(e.nextScene);
			}));

	reloadSceneSub_ = ScopedSubscriber(&GLOBALEVENT,
		GLOBALEVENT.Subscribe<Events::ReloadingSceneEvent>(
			[this](const Events::ReloadingSceneEvent&)
			{
				ReloadScene();
			}));

	// 開始シーンは最初のUpdate()で生成する
	pending_ = currentSceneType_;
}

void SceneManager::Shutdown()
{
	// 先に購読を外して新しい要求を受けないようにする
	sceneChangeSub_.reset();
	reloadSceneSub_.reset();
	pending_.reset();

	DestroyCurrentScene();
}

void SceneManager::Update()
{
	// シーン切替はフレーム先頭でのみ行う
	ApplyPendingTransition();

	if (!currentScene_) return;
	currentScene_->Update(Application::Instance().GetDeltaTime());
}

void SceneManager::PreDraw()
{
	if (!currentScene_) return;
	currentScene_->PreDraw(Application::Instance().GetDeltaTime());
}

void SceneManager::Draw()
{
	if (!currentScene_) return;
	currentScene_->Draw();
}

void SceneManager::DrawSprite()
{
	if (!currentScene_) return;
	currentScene_->DrawSprite();
}

void SceneManager::DrawDebug()
{
	if (!currentScene_) return;
	currentScene_->DrawDebug();
}

void SceneManager::ApplyPendingTransition()
{
	if (!pending_) return;

	const SceneType next = *pending_;
	pending_.reset();

	auto it = factories_.find(next);
	if (it == factories_.end()) return;

	// 旧シーンを完全に破棄してから新シーンを生成する(新旧を同時に存在させない)
	DestroyCurrentScene();

	currentScene_ = it->second();
	currentSceneType_ = next;
	currentScene_->Enter();
}

void SceneManager::DestroyCurrentScene()
{
	if (!currentScene_) return;

	currentScene_->Exit();
	currentScene_.reset();
}