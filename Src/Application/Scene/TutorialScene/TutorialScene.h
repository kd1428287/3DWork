#pragma once

#include "../BaseScene/BaseScene.h"
#include "Application/Core/EventBus/Events/AudioEvents.h"

class EffectDispatcher;
class SlashTrailDispatcher;
class ShapeEffectDispatcher;
class WindShellDispatcher;

class InputSystem;
class TimeScaleSystem;
class CameraSystem;
class ColliderRegistry;
class CollisionSystem;

class TutorialManager;

class TutorialScene : public BaseScene
{
public:

	TutorialScene();
	~TutorialScene() override;

private:

	// BaseScene::Updateは非virtualなので、シーン固有の処理はこちらに書く
	// (systemManager_->Update()が終わった後に呼ばれる)
	void OnUpdate(float deltaTime) override;
	void OnPreDraw(float deltaTime) override;
	void OnDrawEffects() override;
	void OnDrawBright() override;

	// SceneManagerがEnter()経由で呼ぶ。コンストラクタからは呼ばない
	void OnEnter() override;
	void OnExit() override;

	// OnEnter()の内訳。呼び出し順に依存があるので順番を変えない
	void BuildWorld();
	void SetupSlashTrail();	// BuildWorld()内で呼ぶ
	void BuildSystems();
	void SetupEnvironment();
	void SetupTutorial();

	// スキップキーを押した瞬間だけtrue
	bool PollSkip();

	std::unique_ptr<EffectDispatcher> effectDispatcher_ = nullptr;
	std::unique_ptr<SlashTrailDispatcher> slashTrailDispatcher_ = nullptr;
	std::unique_ptr<ShapeEffectDispatcher> shapeEffectDispatcher_ = nullptr;
	std::unique_ptr<WindShellDispatcher> windShellDispatcher_ = nullptr;
	std::unique_ptr<InputSystem> inputSystem_ = nullptr;
	std::unique_ptr<TimeScaleSystem> timeScaleSystem_ = nullptr;
	std::unique_ptr<CameraSystem> cameraSystem_ = nullptr;
	std::unique_ptr<ColliderRegistry> colliderRegistry_ = nullptr;
	std::unique_ptr<CollisionSystem> collisionSystem_ = nullptr;

	// 他のメンバを参照するので末尾に置く(最初に破棄される)
	std::unique_ptr<TutorialManager> tutorialManager_ = nullptr;

	bool prevSkip_ = true;	// 遷移前から押されているキーは無視する
	bool leaving_ = false;	// 遷移要求の多重発行を防ぐ

	SoundHandle bgm_;
};