#pragma once
#include "GaugeBarRenderer.h"
#include "Application/Definitions/UI/GaugeBarStyle.h"
#include "GaugeWatcher.h"

// Playerとは別のGameObjectに付ける、スクリーン固定のHP/体幹バー表示コンポーネント
class PlayerHudGaugeComponent : public ComponentBase, public IRenderable
{
public:
	explicit PlayerHudGaugeComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override
	{
		SceneContext* ctx = GetOwner()->GetContext();
		assert(ctx && ctx->eventBus && "PlayerHudGaugeComponent: eventBusがありません");

		healthSkin_.Load(healthStyle_);
		postureSkin_.Load(postureStyle_);

		watcher_.Init(ctx->eventBus, ctx->player);
	}

	void DrawSprite() override
	{
		KdSpriteShader& shader = KdShaderManager::Instance().m_spriteShader;
		const Math::Vector2 pivot = { 0.0f, 0.0f };

		GaugeBarRenderer::Draw(shader, healthBarPos_, healthBarSize_, watcher_.GetHealthRatio(),
			healthSkin_, kWhiteColor, pivot);
		GaugeBarRenderer::Draw(shader, postureBarPos_, postureBarSize_, watcher_.GetPostureRatio(),
			postureSkin_, kWhiteColor, pivot);
	}

private:
	GaugeWatcher watcher_;

	Math::Vector2 healthBarPos_ = { 160.0f, 50.0f };
	Math::Vector2 healthBarSize_ = { 300.0f, 20.0f };

	Math::Vector2 postureBarPos_ = { 160.0f, 75.0f };
	Math::Vector2 postureBarSize_ = { 300.0f, 10.0f };

	GaugeBarStyle healthStyle_{ "UI/hp_back", "UI/hp_fill", 8 };
	GaugeBarStyle postureStyle_{ "UI/posture_back", "UI/posture_fill", 8 };

	GaugeBarSkin healthSkin_;
	GaugeBarSkin postureSkin_;
};
