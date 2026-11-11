#pragma once

#include "Application/Core/EventBus/Events/CombatEvents.h"

struct InkGaugeConfig {
	float max = 100;
};

class PlayerInkGaugeComponent : public ComponentBase {
public:
	explicit PlayerInkGaugeComponent(GameObject* owner, float max = InkGaugeConfig().max)
		: ComponentBase(owner), max_(max), current_(max) {};

	void Awake() override;

	void AddValue(float value)
	{
		current_ += value;
		current_ = std::clamp(current_, 0.0f, max_);
		
		Events::InkGaugeChanged e;
		e.source = Handle<GameObject>(GetOwner());
		e.ratio = GetRatio();
		GetOwner()->GetSceneEventBus().Publish(e);
	}

	// trueで全消去/falseで全回復
	void GaugeReset(bool isDelete) 
	{
		isDelete ? AddValue(-max_) : AddValue(max_);
	}

	float GetCurrent() const { return current_; }
	float GetMax() const { return max_; }
	float GetRatio() const { return max_ > 0.0f ? current_ / max_ : 0.0f; }

private:
	float max_;
	float current_;
	float chageLine_ = 0.8f;

	bool isCharged_ = false;
};