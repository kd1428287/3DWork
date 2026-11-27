#include "TitleScene.h"
#include "Application/main.h"

#include "Application/Components/Core/TransformComponent.h"

#include "Application/Core/EventBus/Events/SceneEvents.h"
#include "Application/Core/EventBus/Events/TransitionEvents.h"

#include "Application/Factories/Prefab/PrefabFactory.h"

namespace
{
	constexpr float kSplatRadius = 0.05f;	// 画面の高さに対する比率
	constexpr float kSplatDensity = 0.6f;

	constexpr const char* kTitleBgm = "Asset/Sounds/Title/tenkai-yashiro.wav";
	constexpr const char* kClickSe = "Asset/Sounds/Title/wadaiko.wav";

	constexpr float kBgmFadeIn = 1.5f;
	constexpr float kBgmFadeOut = 0.8f;
}

void TitleScene::OnEnter()
{
	LoadUI("Asset/Data/UI/TitleUI.json");

	fluid_.Init();

	systemManager_->SetExecutionOrder(
		[this](float dt) { objManager_->PreUpdate(dt); },
		[this](float dt) { objManager_->Update(dt); },
		[this](float dt) { objManager_->PostUpdate(dt); },
		[this](float dt) { objManager_->Flush(); }
	);

	bgm_ = Events::PublishSoundPlayEvent(*GLOBALEVENT, kTitleBgm, true, 1.0f, 0.0f, kBgmFadeIn);
}

void TitleScene::OnExit()
{
	Events::PublishSoundFadeOutEvent(*GLOBALEVENT, bgm_, kBgmFadeOut);
}

void TitleScene::OnUpdate(float /*deltaTime*/)
{
	AddCursorSplat();
}

void TitleScene::AddCursorSplat()
{
	HWND hwnd = Application::Instance().GetWindowHandle();

	POINT pt;
	RECT rc;
	if (!GetCursorPos(&pt) || !ScreenToClient(hwnd, &pt) || !GetClientRect(hwnd, &rc)) { return; }
	if (rc.right <= 0 || rc.bottom <= 0) { return; }

	Math::Vector2 uv = { static_cast<float>(pt.x) / rc.right, static_cast<float>(pt.y) / rc.bottom };

	// 初回は前回位置が無いので、入力せず位置だけ記録する
	if (hasPrevCursor_)
	{
		Math::Vector2 delta = uv - prevCursorUV_;

		if (delta.LengthSquared() > 0.0f)
		{
			fluid_.AddSplat(uv, delta, kSplatRadius, kSplatDensity);
		}
	}

	prevCursorUV_ = uv;
	hasPrevCursor_ = true;
}

void TitleScene::OnPreDraw(float deltaTime)
{
	fluid_.Step(deltaTime);
}

void TitleScene::OnDrawBackground()
{
	fluid_.Draw();
}
