#pragma once
#include "GaugeBarRenderer.h"
#include "Application/Definitions/UI/GaugeBarStyle.h"
#include "GaugeWatcher.h"
#include "../../../Core/TransformComponent.h"
#include "../../../GamePlay/Camera/CameraComponent.h"

// Enemyとは別のGameObjectに付ける、頭上追従のHP/体幹バー表示コンポーネント
// 対象死亡(HealthDiedEvent)時に自分自身も破棄予約する
class EnemyWorldGaugeComponent : public ComponentBase, public IRenderable
{
public:
	explicit EnemyWorldGaugeComponent(GameObject* owner) : ComponentBase(owner) {}

	void SetTarget(Handle<GameObject> target) { pendingTarget_ = target; }

	void Awake() override
	{
		SceneContext* ctx = GetOwner()->GetContext();
		watcher_.Init(ctx->eventBus, pendingTarget_,
			[this]() { GetOwner()->GetContext()->objectManager->Destroy(GetOwner()); });
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
		Math::Vector2 healthPos = { screenPos3D.x, screenPos3D.y };

		auto hpFrame = KdAssets::Instance().m_textures.GetData(healthStyle_.FrameTexName);
		auto hpFill = KdAssets::Instance().m_textures.GetData(healthStyle_.FillTexName);
		GaugeBarRenderer::Draw(shader, healthPos, healthBarSize_, watcher_.GetHealthRatio(),
			hpFrame.get(), hpFill.get(), kWhiteColor, { 0.5f, 0.0f }, healthStyle_.FrameBorder);

		Math::Vector2 posturePos = { screenPos3D.x, screenPos3D.y + healthBarSize_.y + 2.0f };
		auto postureFrame = KdAssets::Instance().m_textures.GetData(postureStyle_.FrameTexName);
		auto postureFill = KdAssets::Instance().m_textures.GetData(postureStyle_.FillTexName);
		GaugeBarRenderer::Draw(shader, posturePos, postureBarSize_, watcher_.GetPostureRatio(),
			postureFrame.get(), postureFill.get(), kWhiteColor, { 0.5f, 0.0f }, postureStyle_.FrameBorder);
	}

private:
	Handle<GameObject> pendingTarget_;
	GaugeWatcher watcher_;

	Math::Vector3 worldOffset_ = { 0.0f, 2.0f, 0.0f };

	Math::Vector2 healthBarSize_ = { 80.0f, 10.0f };
	Math::Vector2 postureBarSize_ = { 80.0f, 6.0f };

	GaugeBarStyle healthStyle_{ "UI/enemy_hp_back", "UI/enemy_hp_fill", 4 };
	GaugeBarStyle postureStyle_{ "UI/enemy_posture_back", "UI/enemy_posture_fill", 4 };
};
