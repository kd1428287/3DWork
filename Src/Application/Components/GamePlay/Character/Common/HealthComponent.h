#pragma once
#include "Application/Core/EventBus/Events/HealthEvents.h"

// 初期設定(Prefab/JSONから渡す値)。
struct HealthConfig
{
	float max = 100.0f;
};

// HP(体力)を管理する汎用コンポーネント

class HealthComponent : public ComponentBase {
public:
	using Config = HealthConfig;

	explicit HealthComponent(GameObject* owner, float maxHealth = HealthConfig{}.max)
		: ComponentBase(owner), max_(maxHealth), current_(maxHealth) {
	}

	// 最大値を設定し、全回復した状態で始める。
	void SetConfig(const Config& config) { SetMax(config.max, true); }

	// 外部から値を変更する(ダメージ)
	void TakeDamage(float amount) {
		if (amount <= 0.0f || died_) return;

		current_ = std::max(0.0f, current_ - amount);
		PublishChanged();

		if (current_ <= 0.0f) {
			died_ = true;
			HealthDiedEvent e;
			e.source = Handle<GameObject>(GetOwner());
			GetOwner()->GetContext()->eventBus->Publish(e);
			GetOwner()->GetLocalEventBus().Publish(e);
		}
	}

	// 外部から値を変更する(回復)
	void Heal(float amount) {
		if (amount <= 0.0f || died_) return;
		current_ = std::min(max_, current_ + amount);
		PublishChanged();
	}

	// 値を問答無用で上書きしたい場合(復活演出後のリセット、初期値の
	// 個別設定等)に使う。0より大きい値をセットすればdied_も解除される。
	void SetCurrent(float value) {
		current_ = std::clamp(value, 0.0f, max_);
		died_ = (current_ <= 0.0f);
		PublishChanged();
	}

	// 最大値を変更する。alsoHeal=trueなら同時に全回復させる
	void SetMax(float value, bool alsoHeal = false) {
		max_ = value;
		current_ = alsoHeal ? max_ : std::min(current_, max_);
		PublishChanged();
	}

	float GetCurrent() const { return current_; }
	float GetMax() const { return max_; }
	float GetRatio() const { return max_ > 0.0f ? current_ / max_ : 0.0f; }
	bool IsDead() const { return died_; }

private:
	// HPが変化するAPIの末尾で必ず呼び、HealthChangedEventを発行する
	void PublishChanged() {

		PublishHealthChangeEvent(*GetOwner()->GetContext()->eventBus, Handle<GameObject>(GetOwner()), GetRatio());
	}

	float max_;
	float current_;
	bool died_ = false;
};
