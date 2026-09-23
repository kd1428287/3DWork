#pragma once
#include "GaugeBarRenderer.h"
#include "Application/Definitions/UI/GaugeBarStyle.h"
#include "GaugeWatcher.h"
#include "../../../Core/TransformComponent.h"
#include "../../../GamePlay/Camera/CameraComponent.h"

// 敵Prefabの子オブジェクトに付ける、頭上追従のHP/体幹バー表示コンポーネント
// 対象が死亡または消滅したら自分自身も破棄予約する
class EnemyWorldGaugeComponent : public ComponentBase, public IRenderable
{
public:
	explicit EnemyWorldGaugeComponent(GameObject* owner) : ComponentBase(owner) {}

	// AddComponent直後(Startより前)に呼ぶこと
	void SetTarget(Handle<GameObject> target) { pendingTarget_ = target; }

	void Awake() override
	{
		healthSkin_.Load(healthStyle_);
		postureSkin_.Load(postureStyle_);
		healthSkin_.fillColor = healthColor_;
		postureSkin_.fillColor = postureColor_;
	}

	// 対象のHealth等が他オブジェクトのAwakeに依存しないよう、購読はStartで行う
	void Start() override
	{
		assert(pendingTarget_.Resolve() && "EnemyWorldGaugeComponent: SetTargetが呼ばれていません");

		watcher_.Init(&GetOwner()->GetSceneEventBus(), pendingTarget_, [this]() { DestroySelf(); });
	}

	void Update(float deltaTime) override
	{
		// 死亡以外(シーン遷移・デスポーン等)で対象が消えた場合も追従して消える
		if (!pendingTarget_.Resolve()) { DestroySelf(); }
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

	// 枠+背景が1枚の画像、中身は単色(FillTexNameが空)
	GaugeBarStyle healthStyle_{ "UI/enemy_hp_back", "", 4 };
	GaugeBarStyle postureStyle_{ "UI/enemy_posture_back", "", 4 };
	Math::Color healthColor_ = { 0.8f, 0.1f, 0.1f, 1.0f };
	Math::Color postureColor_ = { 0.9f, 0.7f, 0.1f, 1.0f };

	GaugeBarSkin healthSkin_;
	GaugeBarSkin postureSkin_;
};