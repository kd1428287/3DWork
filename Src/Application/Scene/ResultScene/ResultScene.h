#pragma once

#include"../BaseScene/BaseScene.h"

class ResultScene : public BaseScene
{
public:

	ResultScene() = default;
	~ResultScene() override = default;

private:

	// SceneManagerがEnter()経由で呼ぶ。コンストラクタからは呼ばない
	void OnEnter() override;

	// Attack入力でTitleSceneへ遷移する
	void OnUpdate(float deltaTime) override;
};