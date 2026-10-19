#pragma once
#include "GaugeBar.h"
#include "GaugeWatcher.h"
#include "../../../Core/TransformComponent.h"

// 敵オブジェクト自身に付ける、自分のHP/体幹バーをworldOffsetだけずらした位置に描くコンポーネント
// ワールド空間のビルボードポリゴンで描くので、手前のオブジェクトには深度テストで隠れる
// 死亡したらゲージだけ非表示にする(オブジェクトの破棄は他に任せる)
class EnemyOverheadGaugeComponent : public ComponentBase, public IRenderable
{
public:
	// バーの見た目はプレイヤーHUDと同じプリセット(GaugeBarType)をそのまま使う
	explicit EnemyOverheadGaugeComponent(GameObject* owner) : ComponentBase(owner) {}

	// 所有者の位置に足すワールド座標のオフセット(頭上に出す分など)
	void SetWorldOffset(const Math::Vector3& offset) { worldOffset_ = offset; }
	// バーの大きさ(ピクセル指定。pixelsPerMeterでワールドの大きさへ換算する)
	void SetBarSizes(const Math::Vector2& health, const Math::Vector2& posture)
	{
		healthBarSize_ = health;
		postureBarSize_ = posture;
	}
	// 1mあたりのピクセル数。大きいほどバーが小さく出る
	void SetPixelsPerMeter(float pixelsPerMeter) { pixelsPerMeter_ = std::max(pixelsPerMeter, 1.0f); }

	void Awake() override
	{
		healthBar_.LoadWorld();
		postureBar_.LoadWorld();
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

	// 不透明物の描画後に走るエフェクトパスで描く(深度は読むだけで書かない)
	void DrawEffect() override
	{
		if (dead_) return;

		if (!transform_) transform_ = GetOwner()->GetComponent<TransformComponent>();
		if (!transform_) return;

		KdShaderManager& sm = KdShaderManager::Instance();
		KdStandardShader& shader = sm.m_StandardShader;

		// カメラのワールド行列(mViewの逆)の向きのまま、位置だけゲージの基準点にする
		Math::Matrix billboard = sm.GetCameraCB().mView.Invert();
		billboard.Translation(transform_->GetPosition() + worldOffset_);

		const float mpp = 1.0f / pixelsPerMeter_;
		const Math::Vector2 healthSize = healthBarSize_ * mpp;
		const Math::Vector2 postureSize = postureBarSize_ * mpp;

		shader.BeginUnLit();
		sm.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);
		sm.ChangeBlendState(KdBlendState::Alpha);

		// HPバーの上辺を基準点に置き、体幹バーはその2px下へ並べる
		healthBar_.DrawWorld(shader, billboard, { 0.0f, 0.0f }, healthSize, mpp);
		postureBar_.DrawWorld(shader, billboard, { 0.0f, -(healthSize.y + 2.0f * mpp) }, postureSize, mpp);

		sm.UndoBlendState();
		sm.UndoDepthStencilState();
		shader.EndUnLit();
	}

private:
	TransformComponent* transform_ = nullptr;
	GaugeWatcher watcher_;

	GaugeBar healthBar_{ GaugeBarType::Health };
	GaugeBar postureBar_{ GaugeBarType::Posture };

	Math::Vector3 worldOffset_ = { 0.0f, 2.0f, 0.0f };

	Math::Vector2 healthBarSize_ = { 80.0f, 10.0f };
	Math::Vector2 postureBarSize_ = { 80.0f, 6.0f };
	float pixelsPerMeter_ = 100.0f;

	bool dead_ = false;
};
