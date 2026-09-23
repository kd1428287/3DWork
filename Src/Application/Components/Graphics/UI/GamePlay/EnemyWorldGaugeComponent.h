#pragma once
#include "GaugeBarRenderer.h"
#include "Application/Definitions/UI/GaugeBarStyle.h"
#include "GaugeWatcher.h"
#include "../../../Core/TransformComponent.h"
#include "../../../GamePlay/Camera/CameraComponent.h"

// Enemyとは別のGameObjectに付ける、頭上追従のHP/体幹バー表示コンポーネント
// 対象が死亡または消滅したら自分自身も破棄予約する
class EnemyWorldGaugeComponent : public ComponentBase, public IRenderable
{
public:
	explicit EnemyWorldGaugeComponent(GameObject* owner) : ComponentBase(owner) {}

	// Awakeより前に呼ぶこと
	void SetTarget(Handle<GameObject> target) { pendingTarget_ = target; }

	void Awake() override
	{
		SceneContext* ctx = GetOwner()->GetContext();
		assert(ctx && ctx->eventBus && "EnemyWorldGaugeComponent: eventBusがありません");
		assert(pendingTarget_.Resolve() && "EnemyWorldGaugeComponent: SetTargetがAwakeより後です");

		healthSkin_.Load(healthStyle_);
		postureSkin_.Load(postureStyle_);

		watcher_.Init(ctx->eventBus, pendingTarget_, [this]() { DestroySelf(); });
	}

	void Update(float deltaTime) override
	{
		// 死亡以外(シーン遷移・デスポーン等)で対象が消えた場合も追従して消える
		if (!watcher_.GetTarget().Resolve()) { DestroySelf(); }
	}

	void DrawSprite() override
	{
		GameObject* target = watcher_.GetTarget().Resolve();
		if (!target) return;

		TransformComponent* targetTransform = target->GetComponent<TransformComponent>();
		if (!targetTransform) return;

		CameraComponent* camera = GetOwner()->GetContext()->activeCamera;
		if (!camera) return;

		Math::Vector3 screenPos3D;
		camera->GetCamera().ConvertWorldToScreenDetail(targetTransform->GetPosition() + worldOffset_, screenPos3D);
		if (screenPos3D.z <= 0.0f) return; // カメラの後方は描画しない

		KdSpriteShader& shader = KdShaderManager::Instance().m_spriteShader;
		const Math::Vector2 pivot = { 0.5f, 0.0f };

		Math::Vector2 healthPos = { screenPos3D.x, screenPos3D.y };
		GaugeBarRenderer::Draw(shader, healthPos, healthBarSize_, watcher_.GetHealthRatio(),
			healthSkin_, kWhiteColor, pivot);

		Math::Vector2 posturePos = { screenPos3D.x, screenPos3D.y + healthBarSize_.y + 2.0f };
		GaugeBarRenderer::Draw(shader, posturePos, postureBarSize_, watcher_.GetPostureRatio(),
			postureSkin_, kWhiteColor, pivot);
	}

private:
	void DestroySelf()
	{
		GetOwner()->GetContext()->objectManager->Destroy(GetOwner());
	}

	Handle<GameObject> pendingTarget_;
	GaugeWatcher watcher_;

	Math::Vector3 worldOffset_ = { 0.0f, 2.0f, 0.0f };

	Math::Vector2 healthBarSize_ = { 80.0f, 10.0f };
	Math::Vector2 postureBarSize_ = { 80.0f, 6.0f };

	GaugeBarStyle healthStyle_{ "UI/enemy_hp_back", "UI/enemy_hp_fill", 4 };
	GaugeBarStyle postureStyle_{ "UI/enemy_posture_back", "UI/enemy_posture_fill", 4 };

	GaugeBarSkin healthSkin_;
	GaugeBarSkin postureSkin_;
};
