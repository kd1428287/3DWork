#include "TitleScene.h"
#include "../../main.h"

#include "../../Components/Render/SpriteRenderComponent.h"
#include "../../Components/Transform/TransformComponent.h"

// ※ 実際のパスに合わせて調整してください
#include "../../Engine/EventBus/Event/Event.h"
#include "../../Engine/EventBus/Event/SceneEvents.h"

void TitleScene::Init()
{
	BaseScene::Init();

	ShowCursor(true);

	GameObject* logo = objManager_->Instantiate("TitleLogo");
	logo->AddComponent<TransformComponent>()->SetScale({ 1.4f,1.4f,1.4f });
	auto* sprite = logo->AddComponent<SpriteRenderComponent>("Asset/Textures/Title/Title.png");

	systemManager_->SetExecutionOrder(
		[this](float dt) { objManager_->PreUpdate(dt); },
		[this](float dt) { objManager_->Update(dt); },
		[this](float dt) { objManager_->PostUpdate(dt); },
		[this](float dt) { objManager_->Flush(); }
	);
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
