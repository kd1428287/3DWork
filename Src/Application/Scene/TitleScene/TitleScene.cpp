#include "TitleScene.h"
#include "Application/main.h"

#include "Application/Components/Core/TransformComponent.h"

#include "Application/Core/EventBus/Events/SceneEvents.h"

namespace
{
	constexpr float kSplatRadius = 0.05f;	// 画面の高さに対する比率
	constexpr float kSplatDensity = 0.6f;
}

void TitleScene::OnEnter()
{
	GameObject* logo = objManager_->Instantiate("TitleLogo");
	logo->AddComponent<TransformComponent>()->SetScale({ 1.4f,1.4f,1.4f });

	fluid_.Init();

	systemManager_->SetExecutionOrder(
		[this](float dt) { objManager_->PreUpdate(dt); },
		[this](float dt) { objManager_->Update(dt); },
		[this](float dt) { objManager_->PostUpdate(dt); },
		[this](float dt) { objManager_->Flush(); }
	);
}

void TitleScene::OnUpdate(float /*deltaTime*/)
{
	// クリック(Attack)でGameSceneへの遷移をリクエストする。
	// SceneManagerを直接知らなくてよくなった(疎結合)。
	if (KdInputManager::Instance().IsPress("Attack"))
	{
		GLOBALEVENT.Publish(Events::SceneChangeRequestEvent{ SceneType::Game });
	}

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

void TitleScene::OnDrawSprite()
{
	// R16Fなので赤く出る
	std::shared_ptr<KdTexture> tex = fluid_.GetDensityTexture();
	if (!tex) { return; }

	KdShaderManager::Instance().m_spriteShader.DrawTex(tex.get(), 0, 0, 640, 360);
}