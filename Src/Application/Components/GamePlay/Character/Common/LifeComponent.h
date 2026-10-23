#pragma once
#include "Application/Core/EventBus/Events/CombatEvents.h"

struct LifeConfig
{
	int max = 1;
};

class LifeComponent : public ComponentBase {
public:
	explicit LifeComponent(GameObject* owner, int maxLife = 1) : 
		ComponentBase(owner), max_(maxLife) , current_(maxLife) {};

	void Awake() override
	{
		sub_ = GetOwner()->GetLocalEventBus().SubscribeScoped<Events::Down>(
			[this](const Events::Down& e) { Resurrect(); }
		);
	}

	int GetMax() const { return max_; }
	int GetCurrent() const { return current_; }

private:
	void Resurrect()
	{
		auto& bus = GetOwner()->GetLocalEventBus();

		if (max_ > 1) {
			max_--;
			bus.Publish(Events::Resurrect());
		}
		else {
			max_ = 0;
			bus.Publish(Events::Died());
		}
	}

	int max_ = 1;
	int current_ = 1;
	
	ScopedSubscriber sub_;
};