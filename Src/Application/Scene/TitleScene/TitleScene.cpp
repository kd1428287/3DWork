#include "TitleScene.h"
#include "../../main.h"

// ※ 実際のパスに合わせて調整してください
#include "../../Engine/EventBus/Event/Event.h"
#include "../../Engine/EventBus/Event/SceneEvents.h"

void TitleScene::Init()
{
	BaseScene::Init();
}

void TitleScene::OnUpdate(float /*deltaTime*/)
{
	// クリック(Attack)でGameSceneへの遷移をリクエストする。
	// SceneManagerを直接知らなくてよくなった(疎結合)。
	if (KdInputManager::Instance().IsPress("Attack"))
	{
		GLOBALEVENT.Publish(Events::Scene::SceneChangeRequestEvent{ SceneType::Game });
	}
}
