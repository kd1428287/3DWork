#pragma once
#include "Application/Core/EventBus/Events/HealthEvents.h"
#include "Application/Core/EventBus/Events/PostureEvents.h"

class GameObject;

// 対象のHP/体幹イベントをSceneContext::eventBus経由で購読し、最新値を保持するだけのヘルパー
// (対象のローカルバスだと破棄時にダングリングになるため、シーン全体のバスを使う)
class GaugeWatcher
{
public:
	void Init(EventBus* bus, Handle<GameObject> target, std::function<void()> onDied = nullptr)
	{
		target_ = target;
		onDied_ = std::move(onDied);

		healthSub_ = ScopedSubscriber(bus, bus->Subscribe<HealthChangedEvent>(
			[this](const HealthChangedEvent& e) {
				if (e.source != target_) return;
				healthRatio_ = e.ratio;
			}));

		postureSub_ = ScopedSubscriber(bus, bus->Subscribe<PostureChangedEvent>(
			[this](const PostureChangedEvent& e) {
				if (e.source != target_) return;
				postureRatio_ = e.ratio;
			}));

		diedSub_ = ScopedSubscriber(bus, bus->Subscribe<HealthDiedEvent>(
			[this](const HealthDiedEvent& e) {
				if (e.source != target_) return;
				if (onDied_) onDied_();
			}));
	}

	float GetHealthRatio() const { return healthRatio_; }
	float GetPostureRatio() const { return postureRatio_; }
	Handle<GameObject> GetTarget() const { return target_; }

private:
	Handle<GameObject> target_;
	float healthRatio_ = 1.0f;
	float postureRatio_ = 0.0f;

	std::function<void()> onDied_;

	ScopedSubscriber healthSub_;
	ScopedSubscriber postureSub_;
	ScopedSubscriber diedSub_;
};
