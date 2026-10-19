#pragma once
#include "GaugeBar.h"
#include "GaugeWatcher.h"

// UI用オブジェクトに付ける、スクリーン固定の敵HP/体幹バー表示コンポーネント(ボス戦の画面上部用)
// 対象が死亡または消滅したら自分自身も破棄予約する
class EnemyHudGaugeComponent : public ComponentBase, public IRenderable
{
public:
	explicit EnemyHudGaugeComponent(GameObject* owner) : ComponentBase(owner) {}

	// AddComponent直後(Startより前)に呼ぶこと
	void SetTarget(Handle<GameObject> target) { target_ = target; }

	// 位置は画面中心原点、PlayerHudGaugeComponentと同じ座標系(ピクセル)
	void SetHealthRect(const Math::Vector2& pos, const Math::Vector2& size)
	{
		healthBarPos_ = pos;
		healthBarSize_ = size;
	}
	void SetPostureRect(const Math::Vector2& pos, const Math::Vector2& size)
	{
		postureBarPos_ = pos;
		postureBarSize_ = size;
	}

	void Awake() override
	{
		// 敵HPは濃い赤で自機と区別する(Loadより前に上書き)
		healthBar_.Preset().fillColor = { 0.55f, 0.05f, 0.08f, 1.0f };
		healthBar_.Load();
		postureBar_.Load();
	}

	// 対象のHealth等が他オブジェクトのAwakeに依存しないよう、購読はStartで行う
	void Start() override
	{
		assert(target_.Resolve() && "EnemyHudGaugeComponent: SetTargetが呼ばれていません");

		watcher_.Init(&GetOwner()->GetSceneEventBus(), target_, [this]() { DestroySelf(); });
	}

	void Update(float) override
	{
		// 死亡以外(シーン遷移・デスポーン等)で対象が消えた場合も追従して消える
		if (!target_.Resolve()) { DestroySelf(); return; }

		// UIの演出はヒットストップ等に影響されないよう、スケールなしの経過時間で進める
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
	void DestroySelf()
	{
		GetOwner()->GetContext()->objectManager->Destroy(GetOwner());
	}

	Handle<GameObject> target_;
	GaugeWatcher watcher_;

	GaugeBar healthBar_{ GaugeBarType::Health };
	GaugeBar postureBar_{ GaugeBarType::Posture };

	// 左上にHP、上部中央に体幹(中央から左右へ伸びる)
	Math::Vector2 healthBarPos_ = { -600.0f, 300.0f };
	Math::Vector2 healthBarSize_ = { 300.0f, 14.0f };

	Math::Vector2 postureBarPos_ = { -250.0f, 300.0f };
	Math::Vector2 postureBarSize_ = { 500.0f, 10.0f };
};