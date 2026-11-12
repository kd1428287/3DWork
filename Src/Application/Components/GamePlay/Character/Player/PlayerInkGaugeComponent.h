#pragma once

#include "Application/Core/EventBus/Events/CombatEvents.h"

// 初期設定(Prefab/JSONから渡す値)。
struct InkGaugeConfig {
	float max = 100.0f;
	float regenPerSecond = 1.0f;  // 自然回復量/秒
	float gainOnParry = 15.0f;    // パリィ成功時の加算量
	float chargeLine = 0.8f;      // 必殺技が使える割合
};

// プレイヤー専用の必殺技ゲージ。パリィと時間経過で溜まり、必殺技で消費する。
class PlayerInkGaugeComponent : public ComponentBase {
public:
	using Config = InkGaugeConfig;

	explicit PlayerInkGaugeComponent(GameObject* owner, float max = Config{}.max)
		: ComponentBase(owner), max_(max) {}

	void SetConfig(const Config& config) {
		max_ = config.max;
		regenPerSecond_ = config.regenPerSecond;
		gainOnParry_ = config.gainOnParry;
		chargeLine_ = config.chargeLine;
		current_ = std::min(current_, max_);
	}

	void Awake() override {
		// 購読先は実際のHitReactionの発行先に合わせる。
		sub_ = GetOwner()->GetLocalEventBus().SubscribeScoped<Events::HitReaction>(
			[this](const Events::HitReaction& e) { OnHitReaction(e); }
		);
	}

	void Update(float deltaTime) override {
		if (current_ >= max_) return;
		AddValue(regenPerSecond_ * deltaTime);
	}

	// 増減(負で減算)。値が変わった時だけ通知する。
	void AddValue(float value) {
		const float before = current_;
		current_ = std::clamp(current_ + value, 0.0f, max_);
		if (current_ != before) PublishChanged();
	}

	// 足りていれば消費してtrue。必殺技の使用判定はこれ1本で行う。
	bool TryConsume(float cost) {
		if (cost <= 0.0f || current_ < cost) return false;
		AddValue(-cost);
		return true;
	}

	void Fill() { AddValue(max_); }
	void Clear() { AddValue(-max_); }

	// 必殺技が使える状態か(状態を持たず毎回割合から判定する)。
	bool IsCharged() const { return GetRatio() >= chargeLine_; }

	float GetCurrent() const { return current_; }
	float GetMax() const { return max_; }
	float GetRatio() const { return max_ > 0.0f ? current_ / max_ : 0.0f; }

private:
	void OnHitReaction(const Events::HitReaction& e) {
		if (e.type != Events::HitReaction::HitReactionType::Parry) return;
		// ローカルバス購読なら自分の被弾のみ届くので、他者のパリィは来ない。
		AddValue(gainOnParry_);
	}

	void PublishChanged() {
		Events::InkGaugeChanged e;
		e.source = Handle<GameObject>(GetOwner());
		e.ratio = GetRatio();
		GetOwner()->GetSceneEventBus().Publish(e);
	}

	float max_;
	float current_ = 0.0f;
	float regenPerSecond_ = Config{}.regenPerSecond;
	float gainOnParry_ = Config{}.gainOnParry;
	float chargeLine_ = Config{}.chargeLine;

	ScopedSubscriber sub_;
};