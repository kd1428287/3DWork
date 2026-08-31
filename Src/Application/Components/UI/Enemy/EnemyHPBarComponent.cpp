#include "EnemyHPBarComponent.h"
#include "../../Transform/TransformComponent.h" // パスはプロジェクトに合わせて調整
#include "../../Character/Data/PostureComponent.h" // パスはプロジェクトに合わせて調整

void EnemyHPBarComponent::Start()
{
	health_ = GetOwner()->GetComponent<HealthComponent>();
	posture_ = GetOwner()->GetComponent<PostureComponent>(); // 任意。無ければ体幹バーは描画しない
	transform_ = GetOwner()->GetComponent<TransformComponent>();

	if (health_) {
		// EnemyStatusController::Start()のCollisionEnterEvent購読と同じパターン。
		// Subscribe()が返すSubscriptionIdとバスへのポインタをScopedSubscriberに
		// まとめて渡すことで、このコンポーネントの破棄時に自動でUnsubscribeされる。
		EventBus& bus = GetOwner()->GetLocalEventBus();
		SubscriptionId id = bus.Subscribe<HealthComponent::DiedEvent>(
			[this](const HealthComponent::DiedEvent& e) { OnDied(e); });
		diedSubscriber_ = ScopedSubscriber(&bus, id);
	}
}

void EnemyHPBarComponent::OnDied(const HealthComponent::DiedEvent& e)
{
	(void)e;
	isDead_ = true;
	deathFadeElapsed_ = 0.0f;
}

void EnemyHPBarComponent::Update(float deltaTime)
{
	if (!transform_) return;

	if (isDead_) {
		deathFadeElapsed_ += deltaTime;
		if (deathFadeElapsed_ >= deathFadeDuration_) return; // フェード完了後は描画自体をやめる
	}

	if (health_) {
		DrawBar(healthWorldOffset_, healthBarSize_, health_->GetRatio(), healthStyle_);
	}
	if (posture_) {
		DrawBar(postureWorldOffset_, postureBarSize_, posture_->GetRatio(), postureStyle_);
	}
}

void EnemyHPBarComponent::DrawBar(const Math::Vector3& worldOffset, const Math::Vector2& size,
	float ratio, const GaugeBarStyle& style) const
{
	CameraComponent* activeCamera = nullptr;
	if (auto* ctx = GetOwner()->GetContext()) {
		activeCamera = ctx->activeCamera;
	}
	if (!activeCamera) return; // アクティブカメラが未登録なら描画のしようがない

	const Math::Vector3 worldPos = transform_->GetPosition() + worldOffset;

	Math::Vector3 screenResult; // x,y: 画面中心原点のピクセル座標、z: 奥行き(カメラ背後判定に使う)
	activeCamera->GetCamera().ConvertWorldToScreenDetail(worldPos, screenResult);

	// カメラの後方にいる場合は描画しない
	// (ConvertWorldToScreenDetailはwvp._44=zをそのまま返す実装のため、
	// 0以下 or 極端に小さい値なら背後/至近距離とみなす)
	if (screenResult.z <= 0.0f) return;

	// screenResultは画面中心原点。UI用スプライトが左上原点前提の場合は
	// ここでviewport幅高さの半分を加算する変換が必要(要確認)。
	// 例: screenPos.x = screenResult.x + viewportWidth * 0.5f;
	Math::Vector3 screenPos = screenResult;

	GaugeBarStyle drawStyle = style;
	if (isDead_) {
		// テクスチャの透過度を落とす手段(シェーダー側のアルファ乗算等)が
		// 別途必要。ここではフェード進行度だけ計算しておく。
		const float alpha = 1.0f - (deathFadeElapsed_ / deathFadeDuration_);
		(void)alpha; // TODO: シェーダー側にアルファ値を渡すAPIに接続する
	}

	GaugeBarRenderer::Draw(screenPos, size, ratio, drawStyle);
}