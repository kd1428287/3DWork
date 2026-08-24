#pragma once

#include"../BaseScene/BaseScene.h"

class TitleScene : public BaseScene
{
public:

	TitleScene() { Init(); }
	~TitleScene() {}

private:

	void Init()  override;

	// Attack入力でGameSceneへ遷移する
	void OnUpdate(float deltaTime) override;
};