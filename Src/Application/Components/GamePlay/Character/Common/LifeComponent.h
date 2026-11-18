#pragma once
#include "Application/Core/EventBus/Events/CombatEvents.h"

struct LifeConfig
{
	int max = 1;
};

// 残機を管理するコンポーネント。Healthとセットで使う前提。
// current_は今の1人を含む残り人数(1なら復活なし)。
class LifeComponent : public ComponentBase {
public:
	explicit LifeComponent(GameObject* owner, int maxLife = 1) :
		ComponentBase(owner), max_(maxLife), current_(maxLife) {}

	void Awake() override
	{
		sub_ = GetOwner()->GetLocalEventBus().SubscribeScoped<Events::Down>(
			[this](const Events::Down&) { OnDown(); }
		);
	}

	int GetMax() const { return max_; }
	int GetCurrent() const { return current_; }

private:
	// HP0になった時:残機があれば復活、無ければ最終死亡。
	void OnDown()
	{
		auto& bus = GetOwner()->GetLocalEventBus();

		if (current_ > 1) {
			--current_;
			bus.Publish(Events::Resurrect());
			return;
		}

		current_ = 0;

		// 最終死亡はsource付きでシーンとローカルの両方へ通知する。
		Events::Died e;
		e.source = Handle<GameObject>(GetOwner());
		GetOwner()->GetSceneEventBus().Publish(e);
		bus.Publish(e);
	}

	int max_ = 1;
	int current_ = 1;

	ScopedSubscriber sub_;
};