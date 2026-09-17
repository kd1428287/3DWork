#pragma once
#include "GaugeBarRenderer.h"
#include "GaugeWatcher.h"
#include "../../../Core/TransformComponent.h"
#include "../../../GamePlay/Camera/CameraComponent.h"

// ============================================================
// EnemyWorldGaugeComponent
//
// Enemyとは別のGameObject(EnemyFactoryが敵生成と同時に生成)に
// アタッチする、敵の頭上に追従するHP/体幹バー表示専用コンポーネント。
// 対象が死亡した(HealthDiedEvent)ら自分自身を破棄予約する。
// ============================================================
class EnemyWorldGaugeComponent : public ComponentBase, public IRenderable
{
public:
	explicit EnemyWorldGaugeComponent(GameObject* owner) : ComponentBase(owner) {}

	// EnemyFactoryが敵と同時に生成する際に呼ぶ
	void SetTarget(Handle<GameObject> target) { pendingTarget_ = target; }

	void Awake() override
	{
		SceneContext* ctx = GetOwner()->GetContext();
		watcher_.Init(ctx->eventBus, pendingTarget_,
			[this]() {
				// 対象死亡時、このゲージ自身も破棄予約する
				// (予約破棄なので、eventBus自体はこのフレームの
				// Flush()まで生き続けており安全)
				GetOwner()->GetContext()->objectManager->Destroy(GetOwner());
			});
	}

	void DrawSprite() override
	{
		GameObject* target = watcher_.GetTarget().Resolve();
		if (!target) return;

		TransformComponent* targetTransform = target->GetComponent<TransformComponent>();
		if (!targetTransform) return;

		CameraComponent* camera = GetOwner()->GetContext()->activeCamera;
		if (!camera) return;

		Math::Vector3 screenPos;
		camera->GetCamera().ConvertWorldToScreenDetail(targetTransform->GetPosition() + worldOffset_, screenPos);

		// カメラの後方(=画面に映らない)場合は描画しない
		if (screenPos.z <= 0.0f) return;

		// GaugeBarRendererはpivot{0,0}(左上基準)固定のため、
		// バー幅の半分だけ左にずらして頭上中央に来るようにする
		Math::Vector3 healthPos = screenPos;
		healthPos.x -= healthBarSize_.x * 0.5f;
		GaugeBarRenderer::Draw(healthPos, healthBarSize_, watcher_.GetHealthRatio(), healthStyle_);

		Math::Vector3 posturePos = healthPos;
		posturePos.y += healthBarSize_.y + 2.0f;
		GaugeBarRenderer::Draw(posturePos, postureBarSize_, watcher_.GetPostureRatio(), postureStyle_);
	}

private:
	Handle<GameObject> pendingTarget_;
	GaugeWatcher watcher_;

	Math::Vector3 worldOffset_ = { 0.0f, 2.0f, 0.0f }; // 頭上オフセット

	Math::Vector2 healthBarSize_ = { 80.0f, 10.0f };
	Math::Vector2 postureBarSize_ = { 80.0f, 6.0f };

	GaugeBarStyle healthStyle_{ "UI/enemy_hp_back", "UI/enemy_hp_fill" };
	GaugeBarStyle postureStyle_{ "UI/enemy_posture_back", "UI/enemy_posture_fill" };
};
