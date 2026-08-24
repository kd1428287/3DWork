#include "ResultScene.h"
#include "../../main.h"

// ※ 実際のパスに合わせて調整してください
#include "../../Engine/EventBus/Event/Event.h"
#include "../../Engine/EventBus/Event/SceneEvents.h"

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
