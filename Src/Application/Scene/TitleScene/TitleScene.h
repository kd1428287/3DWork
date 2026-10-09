#pragma once

#include"../BaseScene/BaseScene.h"

class TitleScene : public BaseScene
{
public:

	TitleScene() = default;
	~TitleScene() override = default;

private:

	// SceneManagerがEnter()経由で呼ぶ。コンストラクタからは呼ばない
	void OnEnter() override;

	// Attack入力でGameSceneへ遷移する
	void OnUpdate(float deltaTime) override;
};