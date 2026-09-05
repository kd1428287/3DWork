#pragma once
#include <algorithm>
#include "../../ComponentBase.h"

// HP(体力)とは別に「体幹」を管理する汎用コンポーネント

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
	}

	void AddPostureDamage(float amount) {
		if (amount <= 0.0f) return;
		current_ = std::min(max_, current_ + amount);
		regenDelayTimer_ = regenDelaySeconds_;
	}

	// 最大まで溜まっているか(=崩し発生条件)。
	bool IsBroken() const { return current_ >= max_; }

	// 崩し状態を演出し終えた後、体幹をリセットして次の削り合いに備える。
	void Reset() {
		current_ = 0.0f;
		regenDelayTimer_ = 0.0f;
	}

	float GetCurrent() const { return current_; }
	float GetMax() const { return max_; }
	float GetRatio() const { return max_ > 0.0f ? current_ / max_ : 0.0f; }

	void SetRegenPerSecond(float value) { regenPerSecond_ = value; }
	void SetRegenDelaySeconds(float value) { regenDelaySeconds_ = value; }

private:
	float max_;
	float current_;

	// 1秒あたりの自然回復量。
	float regenPerSecond_ = 10.0f;

	// AddPostureDamage()を受けてから回復を再開するまでの秒数。
	float regenDelaySeconds_ = 1.0f;
	float regenDelayTimer_ = 0.0f;
};
