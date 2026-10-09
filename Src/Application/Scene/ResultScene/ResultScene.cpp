#include "ResultScene.h"
#include "Application/main.h"
#include "Application/Core/EventBus/Events/SceneEvents.h"

void ResultScene::OnEnter()
{
	// 固有の初期化はまだ無い(基盤は構築済み)
}

void ResultScene::OnUpdate(float /*deltaTime*/)
{
	// クリック(Attack)でTitleSceneへの遷移をリクエストする。
	if (KdInputManager::Instance().IsPress("Attack"))
	{
		GLOBALEVENT.Publish(Events::SceneChangeRequestEvent{ SceneType::Title });
	}
}