#pragma once
#include "Application/Core/EventBus/Events/CombatEvents.h"

struct LifeConfig
{
	int max = 1;
};

// 残機を管理するコンポーネント。Healthとセットで使う前提。
// current_は今の1人を含む残り人数(1なら復活なし)。
//
// 既定: HP0(Down)で即座にLifeを消費する(プレイヤー等)。
// SetDeferToFatality(true)にすると、Downでは消費せず、致命演出の完了(FatalityFinished)で消費する(敵)。
class LifeComponent : public ComponentBase {
public:
	explicit LifeComponent(GameObject* owner, int maxLife = 1) :
		ComponentBase(owner), max_(maxLife), current_(maxLife) {}

	void Awake() override
	{
		auto& bus = GetOwner()->GetLocalEventBus();
		sub_ = bus.SubscribeScoped<Events::Down>(
			[this](const Events::Down&) { if (!deferToFatality_) ConsumeLife(); }
		);
		fatalitySub_ = bus.SubscribeScoped<Events::FatalityFinished>(
			[this](const Events::FatalityFinished&) { if (deferToFatality_) ConsumeLife(); }
		);
	}

	// trueなら、Life消費のタイミングをDownから致命完了(FatalityFinished)へ移す。
	void SetDeferToFatality(bool defer) { deferToFatality_ = defer; }

	// 演出なしで即死させる(EnemyKillなど)。残機は無視して最終死亡にする。
	void ForceKill()
	{
		current_ = 0;
		PublishDied();
	}

	int GetMax() const { return max_; }
	int GetCurrent() const { return current_; }

private:
	// Lifeを1つ消費する:残機があれば復活、無ければ最終死亡。
	void ConsumeLife()
	{
		if (current_ > 1) {
			--current_;
			GetOwner()->GetLocalEventBus().Publish(Events::Resurrect());
			return;
		}

		current_ = 0;
		PublishDied();
	}

	// 最終死亡はsource付きでシーンとローカルの両方へ通知する。
	void PublishDied()
	{
		Events::Died e;
		e.source = Handle<GameObject>(GetOwner());
		GetOwner()->GetSceneEventBus().Publish(e);
		GetOwner()->GetLocalEventBus().Publish(e);
	}

	int max_ = 1;
	int current_ = 1;
	bool deferToFatality_ = false;

	ScopedSubscriber sub_;
	ScopedSubscriber fatalitySub_;
};