#pragma once

#include"../BaseScene/BaseScene.h"

class ResultScene : public BaseScene
{
public :

	ResultScene()  { Init(); }
	~ResultScene() {}

private :

	void Init()  override;

	// Attack入力でTitleSceneへ遷移する
	void OnUpdate(float deltaTime) override;
};
