#pragma once
#include <algorithm>
#include "../../ComponentBase.h"
#include "../../../Engine/EventBus/Event/Event.h"

// ============================================================
// HealthComponent
//
// HP(体力)を管理する汎用コンポーネント。Player/Enemy問わず、
// ダメージを受ける可能性のあるキャラクターにアタッチする想定
// (PostureComponentと対になる、体幹とは別軸の管理コンポーネント)。
//
// 「誰がどう減らすか」(被弾処理側)と「0になった時に何をするか」
// (死亡演出・破棄処理側)はこのコンポーネント自身の責務ではなく、
// TakeDamage()を呼ぶ側とDiedEventを購読する側にそれぞれ委ねる。
// ============================================================
class HealthComponent : public ComponentBase {
public:
	// このGameObjectのHPが0になった時、ローカルイベントバス
	// (GameObject::GetLocalEventBus())経由で通知されるイベント。
	// EventBus::Publish<T>()の内部実装(pair.second(event)、ハンドラは
	// std::function<void(const Event&)>)がTからEventへの暗黙変換を
	// 要求するため、既存のCollisionEnterEvent等と同様public Eventを
	// 継承する必要がある。
	//
	// 例:
	//   EventBus& bus = enemy->GetLocalEventBus();
	//   bus.Subscribe<HealthComponent::DiedEvent>([](const auto& e) { ... });
	// (EnemyStatusController::Start()のCollisionEnterEvent購読と同じ
	//  パターン。ScopedSubscriberでの寿命管理も同様に行うこと)
	struct DiedEvent : public Event {
		GameObject* object = nullptr;
	};

	explicit HealthComponent(GameObject* owner, float maxHealth = 1000.0f)
		: ComponentBase(owner), max_(maxHealth), current_(maxHealth) {
	}

	// 外部から値を変更する(ダメージ)。0未満にはならない。
	// 既に0(死亡済み)の場合は何もしない(DiedEventの二重発行防止)。
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

	// 外部から値を変更する(回復)。maxを超えない。
	// 死亡済みの状態からの回復は意図しない蘇生になってしまうため、
	// ここでは行わない(復活させたい場合はSetCurrent()を使うこと)。
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
	// (レベルアップ演出等での使用を想定)。
	void SetMax(float value, bool alsoHeal = false) {
		max_ = value;
		current_ = alsoHeal ? max_ : std::min(current_, max_);
	}

	// --- 外部から参照 --------------------------------------------------
	// 例: PostureComponentがGetRatio()を見て、HPが少ないほど体幹の
	// 自然回復速度を落とす、といった連携に使える。
	float GetCurrent() const { return current_; }
	float GetMax() const { return max_; }
	float GetRatio() const { return max_ > 0.0f ? current_ / max_ : 0.0f; }
	bool IsDead() const { return died_; }

private:
	float max_;
	float current_;
	bool died_ = false;
};
