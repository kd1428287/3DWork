#pragma once

#include"../BaseScene/BaseScene.h"

class PlayerFactory;
class CameraFactory;

class InputSystem;

class GameScene : public BaseScene
{
public :

	GameScene();
	~GameScene()override;

	void PreUpdate()override;

private:

	void Event() override;
	void Init()  override;

	std::unique_ptr<PlayerFactory> playerFactory_ = nullptr;
	std::unique_ptr<CameraFactory> cameraFactory_ = nullptr;
	std::unique_ptr<InputSystem> inputSystem_ = nullptr;
};
