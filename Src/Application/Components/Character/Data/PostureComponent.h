#pragma once
#include <algorithm>
#include "../../ComponentBase.h"

// ============================================================
// PostureComponent
//
// HP(体力)とは別に「体幹」を管理する汎用コンポーネント。Player/Enemy
// 問わず、防御(ガード/パリィ)による削り合いを行うキャラクターに
// アタッチする想定(隻狼のようなパリィ主体の防御システム向け)。
//
// 現状はこのコンポーネント単体のデータ管理までで、実際に
// 「いつAddPostureDamage()を呼ぶか」「IsBroken()を見て何をするか」は
// 呼び出し側(PlayerStatusController等)に委ねる。Enemy側は今回
// アタッチのみ行い、削る/削られるロジックへの接続はまだ行わない
// (EnemyStatusController側の攻撃AIが仮実装のため、詳細は別途詰める)。
// ============================================================
class PostureComponent : public ComponentBase {
public:
	explicit PostureComponent(GameObject* owner, float maxPosture = 100.0f)
		: ComponentBase(owner), max_(maxPosture), current_(0.0f) {
	}

	void Update(float deltaTime) override {
		// 被弾直後はしばらく回復を止める(本家の「削られた直後は回復が
		// 遅れる」挙動の簡易版)。regenDelayTimer_が残っている間は
		// 回復させない。
		if (regenDelayTimer_ > 0.0f) {
			regenDelayTimer_ = std::max(0.0f, regenDelayTimer_ - deltaTime);
			return;
		}

		if (current_ <= 0.0f) return;
		current_ = std::max(0.0f, current_ - regenPerSecond_ * deltaTime);
	}

	// 体幹を削る(ガード時の削り、パリィで相手に与える大ダメージ等、
	// 呼び出し側が用途に応じて量を決めて渡す)。
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
