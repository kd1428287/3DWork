#pragma once

#include"../BaseScene/BaseScene.h"
#include "Application/Core/EventBus/Events/AudioEvents.h"
#include "Framework/Shader/PostProcessShader/FluidSimulator.h"

class TitleScene : public BaseScene
{
public:

	TitleScene() = default;
	~TitleScene() override = default;

private:

	// SceneManagerがEnter()経由で呼ぶ。コンストラクタからは呼ばない
	void OnEnter() override;

	// BGMのフェードアウトを保証する(遷移以外の経路で破棄された場合の保険)
	void OnExit() override;

	// Attack入力でGameSceneへ遷移する
	void OnUpdate(float deltaTime) override;

	// 流体はGPUへ描画するため、Updateではなく描画フェーズで進める
	void OnPreDraw(float deltaTime) override;

	// 流体の背景を、スプライトより先に画面全体へ描く
	void OnDrawBackground() override;

	// カーソルの移動を流体への入力にする
	void AddCursorSplat();

	FluidSimulator	fluid_;
	Math::Vector2	prevCursorUV_ = { 0.0f, 0.0f };
	bool			hasPrevCursor_ = false;

	SoundHandle		bgm_;
	bool			leaving_ = false;	// 遷移要求済み(SEとフェードを1回だけ発行する)
};