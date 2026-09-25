#pragma once
#include "GaugeBar.h"
#include "GaugeWatcher.h"

// Prefabの子オブジェクトに付ける、スクリーン固定のプレイヤーHP/体幹バー表示コンポーネント
// 対象はSceneContext::player。Player生成後、最初のStartまでに設定されていること
class PlayerHudGaugeComponent : public ComponentBase, public IRenderable
{
public:
	explicit PlayerHudGaugeComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override
	{
		healthBar_.Load();
		postureBar_.Load();
	}

	// Player生成順に依存しないよう、購読はStartで行う
	void Start() override
	{
		Handle<GameObject> player = GetOwner()->GetContext()->player;
		assert(player.Resolve() && "PlayerHudGaugeComponent: ctx->playerが未設定です");

		watcher_.Init(&GetOwner()->GetSceneEventBus(), player);
	}

	// UIの演出はヒットストップ等に影響されないよう、スケールなしの経過時間で進める
	void Update(float) override
	{
		const float dt = GetOwner()->GetContext()->unscaledDeltaTime;
		healthBar_.Update(dt, watcher_.GetHealthRatio());
		postureBar_.Update(dt, watcher_.GetPostureRatio());
	}

	void DrawSprite() override
	{
		KdSpriteShader& shader = KdShaderManager::Instance().m_spriteShader;
		const Math::Vector2 pivot = { 0.0f, 0.0f };

		healthBar_.Draw(shader, healthBarPos_, healthBarSize_, pivot);
		postureBar_.Draw(shader, postureBarPos_, postureBarSize_, pivot);
	}

private:
	GaugeWatcher watcher_;

	GaugeBar healthBar_{ GaugeBarType::Health };
	GaugeBar postureBar_{ GaugeBarType::Posture };

	Math::Vector2 healthBarPos_ = { 0.0f, 0.0f };
	Math::Vector2 healthBarSize_ = { 300.0f, 20.0f };

	Math::Vector2 postureBarPos_ = { 0.0f, 0.0f };
	Math::Vector2 postureBarSize_ = { 300.0f, 10.0f };
};