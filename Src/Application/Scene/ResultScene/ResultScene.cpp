#include "ResultScene.h"
#include "Application/main.h"
#include "Application/Core/EventBus/Events/SceneEvents.h"

void ResultScene::Init()
{
	BaseScene::Init();
}

void ResultScene::OnUpdate(float /*deltaTime*/)
{
	// クリック(Attack)でTitleSceneへの遷移をリクエストする。
	if (KdInputManager::Instance().IsPress("Attack"))
	{
		GLOBALEVENT.Publish(Events::Scene::SceneChangeRequestEvent{ SceneType::Title });
	}
}
