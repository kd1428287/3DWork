#pragma once
#include "GaugeBar.h"
#include "GaugeWatcher.h"
#include "../../../Core/TransformComponent.h"
#include "../../../GamePlay/Camera/CameraComponent.h"

// 敵Prefabの子オブジェクトに付ける、頭上追従のHP/体幹バー表示コンポーネント
// 対象が死亡または消滅したら自分自身も破棄予約する
class EnemyWorldGaugeComponent : public ComponentBase, public IRenderable
{
public:
	explicit EnemyWorldGaugeComponent(GameObject* owner) : ComponentBase(owner)
	{
		// 敵のHPは小さいバー用の絵に差し替える(絵自体に色があるので色は掛けない)
		GaugeBarPreset& hp = healthBar_.Preset();
		hp.style = GaugeBarStyle{ "UI/enemy_hp_back", "UI/enemy_hp_fill", 4 };
		hp.capsule = false;
		hp.fillColor = kWhiteColor;
	}

	// AddComponent直後(Startより前)に呼ぶこと
	void SetTarget(Handle<GameObject> target) { pendingTarget_ = target; }

	void Awake() override
	{
		healthBar_.Load();
		postureBar_.Load();
	}

	// 対象のHealth等が他オブジェクトのAwakeに依存しないよう、購読はStartで行う
	void Start() override
	{
		assert(pendingTarget_.Resolve() && "EnemyWorldGaugeComponent: SetTargetが呼ばれていません");

		watcher_.Init(&GetOwner()->GetSceneEventBus(), pendingTarget_, [this]() { DestroySelf(); });
	}

	void Update(float) override
	{
		// 死亡以外(シーン遷移・デスポーン等)で対象が消えた場合も追従して消える
		if (!pendingTarget_.Resolve()) { DestroySelf(); return; }

		// UIの演出はヒットストップ等に影響されないよう、スケールなしの経過時間で進める
		const float dt = GetOwner()->GetContext()->unscaledDeltaTime;
		healthBar_.Update(dt, watcher_.GetHealthRatio());
		postureBar_.Update(dt, watcher_.GetPostureRatio());
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
		healthBar_.Draw(shader, healthPos, healthBarSize_, pivot);

		Math::Vector2 posturePos = { screenPos3D.x, screenPos3D.y + healthBarSize_.y + 2.0f };
		postureBar_.Draw(shader, posturePos, postureBarSize_, pivot);
	}

private:
	void DestroySelf()
	{
		GetOwner()->GetContext()->objectManager->Destroy(GetOwner());
	}

	Handle<GameObject> pendingTarget_;
	GaugeWatcher watcher_;

	GaugeBar healthBar_{ GaugeBarType::Health };
	GaugeBar postureBar_{ GaugeBarType::Posture };

	Math::Vector3 worldOffset_ = { 0.0f, 2.0f, 0.0f };

	Math::Vector2 healthBarSize_ = { 80.0f, 10.0f };
	Math::Vector2 postureBarSize_ = { 80.0f, 6.0f };
};