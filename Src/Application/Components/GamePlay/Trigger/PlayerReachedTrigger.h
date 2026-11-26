#pragma once

#include "Application/Core/EventBus/Events/CollisionEvents.h"
#include "Application/Core/EventBus/Events/TriggerEvents.h"

// プレイヤーとのイベント用接触判定
class PlayerReachedTrigger : public ComponentBase {
public:
	explicit PlayerReachedTrigger(GameObject* owner) : ComponentBase(owner) {};

	void Awake() override {
		colSub_ = GetOwner()->GetLocalEventBus().SubscribeScoped<Events::CollisionEnterEvent>(
			[this](const Events::CollisionEnterEvent& e) {
				OnHit(e);
			}
		);
	}

	void OnHit(const Events::CollisionEnterEvent& e) {
		if (!e.SelfIs(ColliderCategory::EventArea)) return;

		GetOwner()->GetSceneEventBus().Publish(Events::PlayerReached());
	}

private:
	ScopedSubscriber colSub_;
};