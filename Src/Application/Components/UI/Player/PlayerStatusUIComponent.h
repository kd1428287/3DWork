#pragma once
#include "../../ComponentBase.h"
#include "../../Render/GaugeBarRenderer.h"

class HealthComponent;
class PostureComponent;

// ============================================================
// PlayerStatusUIComponent
//
// 自機のHP/体幹バーをスクリーン固定位置に表示するUI専用コンポーネント。
// PlayerStatusController(戦闘状態遷移)とは完全に独立させ、
// 同一GameObject上のHealthComponent/PostureComponentをsiblingとして
// 参照し、Update()で毎フレーム値を読んで描画するだけに責務を絞る。
//
// PostureComponentは未アタッチのキャラクターも許容する
// (posture_ == nullptrなら体幹バーの描画をスキップする)。
// ============================================================
class PlayerStatusUIComponent : public ComponentBase
{
public:
	explicit PlayerStatusUIComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override;
	void Update(float deltaTime) override;

private:
	// 同一GameObject上のコンポーネントは生ポインタで保持する
	// (四方向所有ルール: same-GameObject siblingsはraw pointer)
	HealthComponent* health_ = nullptr;
	PostureComponent* posture_ = nullptr; // 未アタッチなら nullptr のまま

	// スクリーン固定のバー配置(値は仮。中心座標+サイズで指定)。
	// 座標系(左上原点/中心原点)は実際のUI用スプライト描画の前提に
	// 合わせて調整すること。
	Math::Vector3 healthBarPos_ = { 160.0f, 50.0f, 0.0f };
	Math::Vector2 healthBarSize_ = { 300.0f, 20.0f };

	Math::Vector3 postureBarPos_ = { 160.0f, 75.0f, 0.0f };
	Math::Vector2 postureBarSize_ = { 300.0f, 10.0f };

	GaugeBarStyle healthStyle_{ "UI/hp_back", "UI/hp_fill" };
	GaugeBarStyle postureStyle_{ "UI/posture_back", "UI/posture_fill" };
};