#include "SceneManager.h"
#include "Application/main.h"

#include "Application/Scene/BaseScene/BaseScene.h"
#include "Application/Scene/TitleScene/TitleScene.h"
#include "Application/Scene/GameScene/GameScene.h"
#include "Application/Scene/ResultScene/ResultScene.h"
#include "Application/Core/EventBus/Events/SceneEvents.h"
#include "Application/Core/EventBus/Events/TransitionEvents.h"

#include "TransitionController.h"

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

	sceneChangeSub_ = GLOBALEVENT->SubscribeScoped<Events::SceneChangeRequest>(
		[this](const Events::SceneChangeRequest& e) {
			RequestSceneChange(e.nextScene, e.withTransition); });

	reloadSceneSub_ = GLOBALEVENT->SubscribeScoped<Events::ReloadingScene>(
		[this](const Events::ReloadingScene& e) {
			ReloadScene(); });

	// 覆い切った通知で、待機中のシーンへ切り替える(待機が無ければ何もしない)
	transitionCoveredSub_ = GLOBALEVENT->SubscribeScoped<Events::TransitionCovered>(
		[this](const Events::TransitionCovered&) {
			if (!waitingScene_) return;

			SetNextScene(*waitingScene_);
			waitingScene_.reset();
		});

	// 開始シーンは最初のUpdate()で生成する
	pending_ = currentSceneType_;
}

void SceneManager::Shutdown()
{
	// 先に購読を外して新しい要求を受けないようにする
	sceneChangeSub_.reset();
	reloadSceneSub_.reset();
	transitionCoveredSub_.reset();
	waitingScene_.reset();
	pending_.reset();

	DestroyCurrentScene();
}

void SceneManager::RequestSceneChange(SceneType next, bool withTransition)
{
	// 遷移待ち・遷移中の要求は無視する(拒否通知が無く、待機が残って固まるのを防ぐ)
	if (waitingScene_ || TransitionController::Instance().IsActive()) return;

	if (!withTransition)
	{
		SetNextScene(next);
		return;
	}

	if (next == currentSceneType_ && !pending_) return;

	waitingScene_ = next;
	GLOBALEVENT->Publish(Events::TransitionRequest{});
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
