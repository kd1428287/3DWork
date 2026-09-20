#pragma once
#include "Application/Core/EventBus/Events/PostureEvents.h"

// 体幹を管理
class PostureComponent : public ComponentBase {
public:
	explicit PostureComponent(GameObject* owner, float maxPosture = 100.0f)
		: ComponentBase(owner), max_(maxPosture), current_(0.0f) {
	}

	void Update(float deltaTime) override {
		// 被弾直後はしばらく回復を止める
		if (regenDelayTimer_ > 0.0f) {
			regenDelayTimer_ = std::max(0.0f, regenDelayTimer_ - deltaTime);
			return;
		}

		if (current_ <= 0.0f) return;

		current_ = std::max(0.0f, current_ - regenPerSecond_ * deltaTime);
		PublishChanged();
	}

	void AddPostureDamage(float amount) {
		if (amount <= 0.0f) return;
		current_ = std::min(max_, current_ + amount);
		regenDelayTimer_ = regenDelaySeconds_;
		PublishChanged();
	}

	// 最大まで溜まっているか(=崩し発生条件)。
	bool IsBroken() const { return current_ >= max_; }

	// 崩し状態を演出し終えた後、体幹をリセットして次の削り合いに備える。
	void Reset() {
		current_ = 0.0f;
		regenDelayTimer_ = 0.0f;
		PublishChanged();
	}

	float GetCurrent() const { return current_; }
	float GetMax() const { return max_; }
	float GetRatio() const { return max_ > 0.0f ? current_ / max_ : 0.0f; }

	void SetLowerLimit(float lowerLimit) { lowerLimit_ = lowerLimit; }
	void SetRegenPerSecond(float regenPerSecond) { regenPerSecond_ = regenPerSecond; }
	void SetRegenDelaySeconds(float regenDelaySeconds) { regenDelaySeconds_ = regenDelaySeconds; }

private:
	void PublishChanged() {
		PostureChangedEvent e;
		e.source = Handle<GameObject>(GetOwner());
		e.ratio = GetRatio();
		GetOwner()->GetContext()->eventBus->Publish(e);
	}

	float max_;
	float current_;

	// 回復できる下限
	float lowerLimit_ = 0.0f;

	// 1秒あたりの自然回復量
	float regenPerSecond_ = 10.0f;

	// AddPostureDamage()を受けてから回復を再開するまでの秒数
	float regenDelaySeconds_ = 1.0f;
	float regenDelayTimer_ = 0.0f;
};
