#pragma once
#include <algorithm>
#include "../../ComponentBase.h"
#include "CharacterEvents.h"

// HP(体力)を管理する汎用コンポーネント

class HealthComponent : public ComponentBase {
public:
	struct DiedEvent : public Event {
		GameObject* object = nullptr;
	};

	explicit HealthComponent(GameObject* owner, float maxHealth = 100.0f)
		: ComponentBase(owner), max_(maxHealth), current_(maxHealth) {
	}

	// 外部から値を変更する(ダメージ)
	void TakeDamage(float amount) {
		if (amount <= 0.0f || died_) return;

		current_ = std::max(0.0f, current_ - amount);

		if (current_ <= 0.0f) {
			died_ = true;
			DiedEvent e;
			e.object = GetOwner();
			GetOwner()->GetLocalEventBus().Publish(e);
		}
	}

	// 外部から値を変更する(回復)
	void Heal(float amount) {
		if (amount <= 0.0f || died_) return;
		current_ = std::min(max_, current_ + amount);
	}

	// 値を問答無用で上書きしたい場合(復活演出後のリセット、初期値の
	// 個別設定等)に使う。0より大きい値をセットすればdied_も解除される。
	void SetCurrent(float value) {
		current_ = std::clamp(value, 0.0f, max_);
		died_ = (current_ <= 0.0f);
	}

	// 最大値を変更する。alsoHeal=trueなら同時に全回復させる
	void SetMax(float value, bool alsoHeal = false) {
		max_ = value;
		current_ = alsoHeal ? max_ : std::min(current_, max_);
	}

	float GetCurrent() const { return current_; }
	float GetMax() const { return max_; }
	float GetRatio() const { return max_ > 0.0f ? current_ / max_ : 0.0f; }
	bool IsDead() const { return died_; }

private:
	float max_;
	float current_;
	bool died_ = false;
};
