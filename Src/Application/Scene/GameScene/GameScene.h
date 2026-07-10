#pragma once

#include"../BaseScene/BaseScene.h"
#include "../../Factories/CardFactory.h"


#include "../../Systems/Card/CardSelectionSystem.h"
#include "../../Systems/Card/HandSystem.h"

class CardFactory;
class PlayerFactory;

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

	std::unique_ptr<CardFactory> cardFactory_ = nullptr;
	std::unique_ptr<PlayerFactory> playerFactory_ = nullptr;
	std::unique_ptr<InputSystem> inputSystem_ = nullptr;
	std::unique_ptr<CardSelectionSystem> cardSelectionSystem_ = nullptr;
	std::unique_ptr<HandSystem> handSystem_ = nullptr;
};
