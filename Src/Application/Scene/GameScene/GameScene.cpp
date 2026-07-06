#include "GameScene.h"
#include"../SceneManager.h"
#include "../../Factories/CardFactory.h"

#include <unordered_map>
#include "../../Components/Card/CardDefinition.h"

void GameScene::Event()
{
	if (GetAsyncKeyState('T') & 0x8000)
	{
		SceneManager::Instance().SetNextScene
		(
			SceneManager::SceneType::Title
		);
	}
}

void GameScene::Init()
{
	BaseScene::Init();
	objManager_->Instantiate("Player");
	CardDefinition def;
	std::unordered_map<std::string, CardDefinition> map;
	map.emplace("Attack", def);
	cardFactory_ = std::make_unique<CardFactory>(
		map
	);
	cardFactory_->CreateCard(*objManager_, "Attack", 0);
}
