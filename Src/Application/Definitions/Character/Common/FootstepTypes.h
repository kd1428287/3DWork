#pragma once

// ============================================================
// FootstepTypes
// PlayerMovementAnimationComponent(アニメーション再生側)と
// FootDustComponent(演出側)を疎結合にするための共通データ定義。
// 配置場所: Application/Components/Graphics/Animation/
// ============================================================

enum class FootSide { Left, Right };

// 1クリップ内での接地タイミングを正規化再生位置(0.0〜1.0)で表す。
// 実時間ではなく比率で持つことで、Start/Endのように速度スケーリング
// されるクリップでも接地の比率が崩れない。
struct FootstepTrigger
{
	float phase = 0.0f; // 0.0〜1.0
	FootSide foot = FootSide::Left;
};

// 接地phaseを通過した瞬間にEventBusへ流すイベント。
struct FootstepEvent : public Event
{
	GameObject* owner = nullptr;
	FootSide foot = FootSide::Left;
};

inline void PublishFootstepEvent(EventBus& bus, GameObject* owner, const FootSide& foot) 
{
	FootstepEvent e;
	e.owner = owner;
	e.foot = foot;
	bus.Publish(e);
}

