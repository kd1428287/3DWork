#pragma once
#include <algorithm>
#include "Application/Core/EventBus/Events/CombatEvents.h"

// Prefab/JSONから渡す初期設定。
struct HealthConfig
{
	float max = 100.0f;
};

// HPを管理する汎用コンポーネント。Lifeとセットで使う前提。
class HealthComponent : public ComponentBase {
public:
	using Config = HealthConfig;

	explicit HealthComponent(GameObject* owner, float maxHealth = HealthConfig{}.max)
		: ComponentBase(owner), max_(maxHealth), current_(maxHealth) {}

	void Awake() override
	{
		sub_ = GetOwner()->GetLocalEventBus().SubscribeScoped<Events::Resurrect>(
			[this](const Events::Resurrect&) { Resurrect(); }
		);
	}

	// 最大値を設定し、全回復した状態で始める。
	void SetConfig(const Config& config) { SetMax(config.max, true); }

	void TakeDamage(float amount)
	{
		if (amount <= 0.0f || died_) return;

		current_ = std::max(0.0f, current_ - amount);
		PublishChanged();

		if (current_ <= 0.0f) {
			died_ = true;
			PublishDown();
		}
	}

	void Heal(float amount)
	{
		if (amount <= 0.0f || died_) return;
		current_ = std::min(max_, current_ + amount);
		PublishChanged();
	}

	// 値を強制上書きする。0より大きければdied_も解除される。
	void SetCurrent(float value)
	{
		const bool wasDead = died_;
		current_ = std::clamp(value, 0.0f, max_);
		died_ = (current_ <= 0.0f);
		PublishChanged();

		// 生存→死亡に変わった時だけDownを出す。
		if (!wasDead && died_) PublishDown();
	}

	// 最大値を変更する。alsoHeal=trueなら全回復。
	void SetMax(float value, bool alsoHeal = false)
	{
		max_ = std::max(1.0f, value);
		current_ = alsoHeal ? max_ : std::min(current_, max_);
		died_ = (current_ <= 0.0f);
		PublishChanged();
	}

	float GetCurrent() const { return current_; }
	float GetMax() const { return max_; }
	float GetRatio() const { return max_ > 0.0f ? current_ / max_ : 0.0f; }
	bool IsDead() const { return died_; }

private:
	// HP変化APIの末尾で呼び、HealthChangedを発行する。
	void PublishChanged()
	{
		Events::PublishHealthChangeEvent(GetOwner()->GetSceneEventBus(), Handle<GameObject>(GetOwner()), GetRatio());
	}

	// HP0通知。最終的な死亡(Died)はLifeが判断する。
	void PublishDown()
	{
		Events::Down e;
		GetOwner()->GetSceneEventBus().Publish(e);
		GetOwner()->GetLocalEventBus().Publish(e);
	}

	// 復活:死亡状態を解除して全回復する。
	void Resurrect()
	{
		died_ = false;
		current_ = max_;
		PublishChanged();
	}

	float max_;
	float current_;
	bool died_ = false;

	ScopedSubscriber sub_;
};