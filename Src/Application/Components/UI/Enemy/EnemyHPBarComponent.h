#pragma once
#include "../../ComponentBase.h"
#include "../../../Engine/EventBus/EventBus.h" // ScopedSubscriberの完全な定義が必要なためinclude
#include "../../Render/GaugeBarRenderer.h"
#include "../../Character/Data/HealthComponent.h" // DiedEventの型を使うため

class TransformComponent;
class PostureComponent;

// ============================================================
// EnemyHPBarComponent
//
// 敵の頭上にワールド追従(ビルボード)するHP/体幹バーを表示するUI専用
// コンポーネント。PlayerStatusUIComponentと役割は同じ(HealthComponent/
// PostureComponentを読んで描画するだけ)だが、「スクリーン固定 vs
// ワールド追従」「死亡時のフェードアウト演出」が異なるため別クラスとして
// 分離している(共通する描画部分はGaugeBarRendererに集約済み)。
//
// PostureComponentは未アタッチのキャラクターも許容する
// (posture_ == nullptrなら体幹バーの描画をスキップする。
//  PlayerStatusUIComponentと同じ方針)。
//
// ワールド→スクリーン変換はKdCamera::ConvertWorldToScreenDetail()を
// GetContext()->activeCamera経由で呼び出す。この関数は画面中心を
// 原点としたピクセル座標を返す実装になっているため、UI用スプライトが
// 左上原点を前提にしている場合はviewport幅高さの半分を加算する変換が
// 別途必要になる(要確認・要調整)。
//
// HealthComponent::DiedEventを購読し、死亡直後は即座に消すのではなく
// deathFadeDuration_かけてフェードアウトさせてから描画を止める
// (HP/体幹バー両方に共通で適用する)。
// ============================================================
class EnemyHPBarComponent : public ComponentBase
{
public:
	explicit EnemyHPBarComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override;
	void Update(float deltaTime) override;

	// 9スライスを使いたい場合はStart()後にこれらを呼んで設定を上書きする。
	// (呼ばなければデフォルトのGaugeBarStyle、つまり単純引き伸ばし描画のまま)
	void SetHealthStyle(const GaugeBarStyle& style) { healthStyle_ = style; }
	void SetPostureStyle(const GaugeBarStyle& style) { postureStyle_ = style; }

private:
	void OnDied(const HealthComponent::DiedEvent& e);

	// HP/体幹共通の「ワールド座標→スクリーン座標変換→カメラ背後判定→
	// 描画」の流れをまとめた内部ヘルパー。
	void DrawBar(const Math::Vector3& worldOffset, const Math::Vector2& size,
		float ratio, const GaugeBarStyle& style) const;

	HealthComponent* health_ = nullptr;
	PostureComponent* posture_ = nullptr; // 未アタッチなら nullptr のまま
	TransformComponent* transform_ = nullptr; // ワールド座標取得用(same-GameObject sibling)

	// DiedEvent購読の寿命管理。GetOwner()->GetLocalEventBus()から
	// 発行されたSubscriptionIdとバスへのポインタをセットで保持し、
	// 破棄時に自動でUnsubscribeされる。
	ScopedSubscriber diedSubscriber_;

	// 頭上オフセット(モデルの高さに応じて要調整)。体幹バーはHPバーの
	// 少し下に表示する。
	Math::Vector3 healthWorldOffset_ = { 0.0f, 2.0f, 0.0f };
	Math::Vector3 postureWorldOffset_ = { 0.0f, 1.8f, 0.0f };

	// バーの見た目サイズ(スクリーンピクセル)。体幹バーはHPバーより
	// 薄くしている(PlayerStatusUIComponentの比率と合わせた)。
	Math::Vector2 healthBarSize_ = { 120.0f, 12.0f };
	Math::Vector2 postureBarSize_ = { 120.0f, 6.0f };

	GaugeBarStyle healthStyle_{ "UI/enemy_hp_back", "UI/enemy_hp_fill" };
	GaugeBarStyle postureStyle_{ "UI/enemy_posture_back", "UI/enemy_posture_fill" };

	// 死亡後のフェードアウト管理(HP/体幹バー共通)
	bool isDead_ = false;
	float deathFadeElapsed_ = 0.0f;
	float deathFadeDuration_ = 0.6f;
};