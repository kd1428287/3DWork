#pragma once

#include"../BaseScene/BaseScene.h"
#include "Framework/Shader/PostProcessShader/FluidSimulator.h"

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

	// 流体はGPUへ描画するため、Updateではなく描画フェーズで進める
	void OnPreDraw(float deltaTime) override;

	// 確認用の密度RT表示(ステップ6の表示シェーダーで置き換える)
	void OnDrawSprite() override;

	// カーソルの移動を流体への入力にする
	void AddCursorSplat();

	FluidSimulator	fluid_;
	Math::Vector2	prevCursorUV_ = { 0.0f, 0.0f };
	bool			hasPrevCursor_ = false;
};