#include "SceneManager.h"
#include "../main.h"

#include "BaseScene/BaseScene.h"
#include "TitleScene/TitleScene.h"
#include "GameScene/GameScene.h"
#include "ResultScene/ResultScene.h"

// ※ 実際のパスに合わせて調整してください
#include "../Engine/EventBus/Event/Event.h"
#include "../Engine/EventBus/Event/SceneEvents.h"

void SceneManager::Init()
{
	// Events::Scene::SceneChangeRequestEvent の購読。
	// 「敵を倒した」「タイマーが切れた」等、"何が遷移条件か"はここでは
	// 一切判断せず、受け取ったSceneTypeへSetNextSceneするだけにする。
	// SceneManagerはアプリ生存期間ずっと生きるシングルトンなので、
	// 明示的なUnsubscribeがなくても問題ないが、ScopedSubscriberで
	// 正しく解除できる形にしておく。
	SubscriptionId id = GLOBALEVENT.Subscribe<Events::Scene::SceneChangeRequestEvent>(
		[this](const Events::Scene::SceneChangeRequestEvent& e)
		{
			SetNextScene(e.nextScene);
		});
	m_sceneChangeSub = std::make_unique<ScopedSubscriber>(&GLOBALEVENT, id);

	// 開始シーンに切り替え
	ChangeScene(m_currentSceneType);
}

SceneManager::~SceneManager() = default;

void SceneManager::Update()
{
	// シーン切替
	if (m_currentSceneType != m_nextSceneType)
	{
		ChangeScene(m_nextSceneType);
	}

	m_currentScene->Update(Application::Instance().GetDeltaTime());
}

void SceneManager::PreDraw()
{
	m_currentScene->PreDraw(Application::Instance().GetDeltaTime());
}

void SceneManager::Draw()
{
	m_currentScene->Draw();
}

void SceneManager::DrawSprite()
{
	m_currentScene->DrawSprite();
}

void SceneManager::DrawDebug()
{
	m_currentScene->DrawDebug();
}

void SceneManager::ChangeScene(SceneType _sceneType)
{
	// 次のシーンを作成し、現在のシーンにする
	switch (_sceneType)
	{
	case SceneType::Title:
		m_currentScene = std::make_shared<TitleScene>();
		break;
	case SceneType::Game:
		m_currentScene = std::make_shared<GameScene>();
		break;
	case SceneType::Result:
		m_currentScene = std::make_shared<ResultScene>();
		break;
	}

	//m_currentScene->Init();
	// 現在のシーン情報を更新
	m_currentSceneType = _sceneType;
}
