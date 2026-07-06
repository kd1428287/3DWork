#pragma once

#include"../BaseScene/BaseScene.h"
#include "../../Factories/CardFactory.h"


class CardFactory;

class GameScene : public BaseScene
{
public :

	GameScene()  { Init(); }
	~GameScene() {}

private:

	void Event() override;
	void Init()  override;

	std::unique_ptr<CardFactory> cardFactory_ = nullptr;
};
