#pragma once

#include"../BaseScene/BaseScene.h"

class EffectDispatcher;
class SlashTrailDispatcher;

class InputSystem;
class TimeScaleSystem;
class CameraSystem;
class ColliderRegistry;
class CollisionSystem;
class RaycastSystem;

class GameScene : public BaseScene
{
public:

	GameScene();
	~GameScene()override;

private:

	// BaseScene::Updateは非virtualなので、シーン固有の処理はこちらに書く
	// (systemManager_->Update()が終わった後に呼ばれる)
	void OnUpdate(float deltaTime) override;
	void OnPreDraw(float deltaTime) override;
	void OnDrawEffects() override;
	void OnDrawBright() override;

	// SceneManagerがEnter()経由で呼ぶ。コンストラクタからは呼ばない
	void OnEnter() override;

	// OnEnter()の内訳。呼び出し順に依存があるので順番を変えない
	void BuildWorld();
	void BuildSystems();
	void SetupEnvironment();

	std::unique_ptr<EffectDispatcher> effectDispatcher_ = nullptr;
	std::unique_ptr<SlashTrailDispatcher> slashTrailDispatcher_ = nullptr;
	std::unique_ptr<InputSystem> inputSystem_ = nullptr;
	std::unique_ptr<TimeScaleSystem> timeScaleSystem_ = nullptr;
	std::unique_ptr<CameraSystem> cameraSystem_ = nullptr;
	std::unique_ptr<ColliderRegistry> colliderRegistry_ = nullptr;
	std::unique_ptr<CollisionSystem> collisionSystem_ = nullptr;
};