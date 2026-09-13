#include "SceneManager.h"
#include "Application/main.h"

#include "Application/Scene/BaseScene/BaseScene.h"
#include "Application/Scene/TitleScene/TitleScene.h"
#include "Application/Scene/GameScene/GameScene.h"
#include "Application/Scene/ResultScene/ResultScene.h"
#include "Application/Core/EventBus/Events/SceneEvents.h"

void SceneManager::Init()
{
	SubscriptionId id = GLOBALEVENT.Subscribe<Events::Scene::SceneChangeRequestEvent>(
		[this](const Events::Scene::SceneChangeRequestEvent& e)
		{
			SetNextScene(e.nextScene);
		});
	sceneChangeSub_ = std::make_unique<ScopedSubscriber>(&GLOBALEVENT, id);

	id = GLOBALEVENT.Subscribe<Events::Scene::ReloadingSceneEvent>(
		[this](const Events::Scene::ReloadingSceneEvent& e)
		{
			ChangeScene(currentSceneType_);
		});
	reloadSceneSub_ = std::make_unique<ScopedSubscriber>(&GLOBALEVENT, id);

	// 開始シーンに切り替え
	ChangeScene(currentSceneType_);
}

SceneManager::~SceneManager() = default;

void SceneManager::Update()
{
	// シーン切替
	if (currentSceneType_ != nextSceneType_)
	{
		ChangeScene(nextSceneType_);
	}

	currentScene_->Update(Application::Instance().GetDeltaTime());
}

void SceneManager::PreDraw()
{
	currentScene_->PreDraw(Application::Instance().GetDeltaTime());
}

void SceneManager::Draw()
{
	currentScene_->Draw();
}

void SceneManager::DrawSprite()
{
	currentScene_->DrawSprite();
}

void SceneManager::DrawDebug()
{
	currentScene_->DrawDebug();
}

void SceneManager::ChangeScene(SceneType _sceneType)
{
	// 次のシーンを作成し、現在のシーンにする
	switch (_sceneType)
	{
	case SceneType::Title:
		currentScene_ = std::make_shared<TitleScene>();
		break;
	case SceneType::Game:
		currentScene_ = std::make_shared<GameScene>();
		break;
	case SceneType::Result:
		currentScene_ = std::make_shared<ResultScene>();
		break;
	}

	//currentScene->Init();
	// 現在のシーン情報を更新
	currentSceneType_ = _sceneType;
}
