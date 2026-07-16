#pragma once

#include"../BaseScene/BaseScene.h"

class PlayerFactory;
class EnemyFactory;
class CameraFactory;
class TerrainFactory;

class InputSystem;
class ColliderRegistry;
class CollisionSystem;
class RaycastSystem;

class GameScene : public BaseScene
{
public:

	GameScene();
	~GameScene()override;

private:

	// BaseScene::Updateは非virtualになったため、シーン固有の処理はこちらに書く
	// (systemManager_->Update()が終わった後に呼ばれる)
	void OnUpdate(float deltaTime) override;
	void Init()  override;

	std::unique_ptr<PlayerFactory> playerFactory_ = nullptr;
	std::unique_ptr<EnemyFactory> enemyFactory_ = nullptr;
	std::unique_ptr<CameraFactory> cameraFactory_ = nullptr;
	std::unique_ptr<TerrainFactory> terrainFactory_ = nullptr;
	std::unique_ptr<InputSystem> inputSystem_ = nullptr;
	std::unique_ptr<ColliderRegistry> colliderRegistry_ = nullptr;
	std::unique_ptr<CollisionSystem> collisionSystem_ = nullptr;
};