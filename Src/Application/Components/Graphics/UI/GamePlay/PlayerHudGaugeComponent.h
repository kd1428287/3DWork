#pragma once
#include "GaugeBarRenderer.h"
#include "GaugeWatcher.h"
#include "../../../GamePlay/Camera/CameraComponent.h"

// ============================================================
// PlayerHudGaugeComponent
//
// Playerとは別のGameObject(UIオブジェクト)にアタッチする、
// スクリーン固定のHP/体幹バー表示専用コンポーネント。
// SceneContext::playerからPlayerを見つけ、GaugeWatcherで
// イベント購読するだけの薄い責務に絞る(値のポーリングは行わない)。
// ============================================================
class PlayerHudGaugeComponent : public ComponentBase, public IRenderable
{
public:
	explicit PlayerHudGaugeComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override
	{
		SceneContext* ctx = GetOwner()->GetContext();
		watcher_.Init(ctx->eventBus, ctx->player);
	}

	void DrawSprite() override
	{
		GaugeBarRenderer::Draw(healthBarPos_, healthBarSize_, watcher_.GetHealthRatio(), healthStyle_);
		GaugeBarRenderer::Draw(postureBarPos_, postureBarSize_, watcher_.GetPostureRatio(), postureStyle_);
	}

private:
	GaugeWatcher watcher_;

	// スクリーン固定のバー配置(値は仮。中心座標+サイズで指定)。
	Math::Vector3 healthBarPos_ = { 160.0f, 50.0f, 0.0f };
	Math::Vector2 healthBarSize_ = { 300.0f, 20.0f };

	Math::Vector3 postureBarPos_ = { 160.0f, 75.0f, 0.0f };
	Math::Vector2 postureBarSize_ = { 300.0f, 10.0f };

	GaugeBarStyle healthStyle_{ "UI/bar.png", "UI/bar.png" };
	GaugeBarStyle postureStyle_{ "UI/bar.png", "UI/bar.png" };
};
