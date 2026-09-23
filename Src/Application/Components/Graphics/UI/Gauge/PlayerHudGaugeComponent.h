#pragma once
#include "GaugeBarRenderer.h"
#include "Application/Definitions/UI/GaugeBarStyle.h"
#include "GaugeWatcher.h"

// 独立Prefab(Hud.json)に付ける、スクリーン固定のプレイヤーHP/体幹バー表示コンポーネント
// 対象はSceneContext::player。Player生成後、最初のStartまでに設定されていること
class PlayerHudGaugeComponent : public ComponentBase, public IRenderable
{
public:
	explicit PlayerHudGaugeComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override
	{
		healthSkin_.Load(healthStyle_);
		postureSkin_.Load(postureStyle_);
		healthSkin_.fillColor = healthColor_;
		postureSkin_.fillColor = postureColor_;
		healthSkin_.capsule = true;
	}

	// Player生成順に依存しないよう、購読はStartで行う
	void Start() override
	{
		Handle<GameObject> player = GetOwner()->GetContext()->player;
		assert(player.Resolve() && "PlayerHudGaugeComponent: ctx->playerが未設定です");

		watcher_.Init(&GetOwner()->GetSceneEventBus(), player);
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

	// HPはカプセル型の枠+中身の画像(3分割描画)、体幹は枠+背景1枚と単色の中身
	GaugeBarStyle healthStyle_{ "Asset/Textures/UI/hp_back.png", "Asset/Textures/UI/hp_fill.png", 0 };
	GaugeBarStyle postureStyle_{ "Asset/Textures/UI/posture_back.png", "", 8 };
	Math::Color healthColor_ = { 0.8f, 0.1f, 0.1f, 1.0f };
	Math::Color postureColor_ = { 0.9f, 0.7f, 0.1f, 1.0f };

	GaugeBarSkin healthSkin_;
	GaugeBarSkin postureSkin_;
};