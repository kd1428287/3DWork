#pragma once
#include "GaugeBar.h"
#include "GaugeWatcher.h"
#include "../../../Core/TransformComponent.h"
#include "../../../GamePlay/Camera/CameraComponent.h"

// 敵オブジェクト自身に付ける、自分のHP/体幹バーをworldOffsetだけずらした位置に描くコンポーネント
// 死亡したらゲージだけ非表示にする(オブジェクトの破棄は他に任せる)
class EnemyOverheadGaugeComponent : public ComponentBase, public IRenderable
{
public:
	// バーの見た目はプレイヤーHUDと同じプリセット(GaugeBarType)をそのまま使う
	explicit EnemyOverheadGaugeComponent(GameObject* owner) : ComponentBase(owner) {}

	// 所有者の位置に足すワールド座標のオフセット(頭上に出す分など)
	void SetWorldOffset(const Math::Vector3& offset) { worldOffset_ = offset; }
	// バーの大きさ(ピクセル)
	void SetBarSizes(const Math::Vector2& health, const Math::Vector2& posture)
	{
		healthBarSize_ = health;
		postureBarSize_ = posture;
	}

	void Awake() override
	{
		healthBar_.Load();
		postureBar_.Load();
	}

	// 自分のHealth等が他コンポーネントのAwakeに依存しないよう、購読はStartで行う
	void Start() override
	{
		watcher_.Init(&GetOwner()->GetSceneEventBus(), Handle<GameObject>(GetOwner()), [this]() { dead_ = true; });
	}

	void Update(float) override
	{
		// UIの演出はヒットストップ等に影響されないよう、スケールなしの経過時間で進める
		const float dt = GetOwner()->GetContext()->unscaledDeltaTime;
		healthBar_.Update(dt, watcher_.GetHealthRatio());
		postureBar_.Update(dt, watcher_.GetPostureRatio());
	}

	void DrawSprite() override
	{
		if (dead_) return;

		if (!transform_) transform_ = GetOwner()->GetComponent<TransformComponent>();
		if (!transform_) return;

		CameraComponent* camera = GetOwner()->GetContext()->activeCamera;
		if (!camera) return;

		Math::Vector3 screenPos3D;
		camera->GetCamera().ConvertWorldToScreenDetail(transform_->GetPosition() + worldOffset_, screenPos3D);
		if (screenPos3D.z <= 0.0f) return; // カメラの後方は描画しない

		KdSpriteShader& shader = KdShaderManager::Instance().m_spriteShader;
		const Math::Vector2 pivot = { 0.5f, 0.0f };

		Math::Vector2 healthPos = { screenPos3D.x, screenPos3D.y };
		healthBar_.Draw(shader, healthPos, healthBarSize_, pivot);

		Math::Vector2 posturePos = { screenPos3D.x, screenPos3D.y + healthBarSize_.y + 2.0f };
		postureBar_.Draw(shader, posturePos, postureBarSize_, pivot);
	}

private:
	TransformComponent* transform_ = nullptr;
	GaugeWatcher watcher_;

	GaugeBar healthBar_{ GaugeBarType::Health };
	GaugeBar postureBar_{ GaugeBarType::Posture };

	Math::Vector3 worldOffset_ = { 0.0f, 2.0f, 0.0f };

	Math::Vector2 healthBarSize_ = { 80.0f, 10.0f };
	Math::Vector2 postureBarSize_ = { 80.0f, 6.0f };

	bool dead_ = false;
};