#pragma once
#include "../../ComponentBase.h"
#include "../../../Engine/EventBus/EventBus.h" // ScopedSubscriberの完全な定義が必要なためinclude
#include "../../Render/GaugeBarRenderer.h"
#include "../../Character/Data/HealthComponent.h" // DiedEventの型を使うため

class TransformComponent;

// ============================================================
// EnemyHPBarComponent
//
// 敵の頭上にワールド追従(ビルボード)するHPバーを表示するUI専用
// コンポーネント。PlayerStatusUIComponentと役割は同じ(HealthComponent
// を読んで描画するだけ)だが、「スクリーン固定 vs ワールド追従」
// 「死亡時のフェードアウト演出」が異なるため別クラスとして分離している
// (共通する描画部分はGaugeBarRendererに集約済み)。
//
// ワールド→スクリーン変換はKdCamera::ConvertWorldToScreenDetail()を
// GetContext()->activeCamera経由で呼び出す。この関数は画面中心を
// 原点としたピクセル座標を返す実装になっているため、UI用スプライトが
// 左上原点を前提にしている場合はviewport幅高さの半分を加算する変換が
// 別途必要になる(要確認・要調整)。
//
// HealthComponent::DiedEventを購読し、死亡直後は即座に消すのではなく
// deathFadeDuration_かけてフェードアウトさせてから描画を止める。
// ============================================================
class EnemyHPBarComponent : public ComponentBase
{
public:
	explicit EnemyHPBarComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override;
	void Update(float deltaTime) override;

private:
	void OnDied(const HealthComponent::DiedEvent& e);

	HealthComponent* health_ = nullptr;
	TransformComponent* transform_ = nullptr; // ワールド座標取得用(same-GameObject sibling)

	// DiedEvent購読の寿命管理。GetOwner()->GetLocalEventBus()から
	// 発行されたSubscriptionIdとバスへのポインタをセットで保持し、
	// 破棄時に自動でUnsubscribeされる。
	ScopedSubscriber diedSubscriber_;

	// 頭上オフセット(モデルの高さに応じて要調整)
	Math::Vector3 worldOffset_ = { 0.0f, 2.0f, 0.0f };

	// バーの見た目サイズ(スクリーンピクセル)
	Math::Vector2 barSize_ = { 120.0f, 12.0f };

	GaugeBarStyle style_{ "UI/enemy_hp_back", "UI/enemy_hp_fill" };

	// 死亡後のフェードアウト管理
	bool isDead_ = false;
	float deathFadeElapsed_ = 0.0f;
	float deathFadeDuration_ = 0.6f;
};