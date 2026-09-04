#pragma once

class TimeScaleSystem
{
public:
	explicit TimeScaleSystem(EventBus& bus, ObjectManager& objManager) : eventBus_(bus), objManager_(objManager) 
	{
	};

	void Update(float dt)
	{
		elapsed_ -= dt;
		if (elapsed_ <= 0.f)
		{
			objManager_.SetTimeScale(1.f, ObjectFlags::Gameplay);
		}
	}

	void SetHitStop();

private:
	EventBus& eventBus_;
	ObjectManager& objManager_;
	std::vector<ScopedSubscriber> subscriptions_;

	std::unordered_map<float, uint8_t> timeScales_;

	float elapsed_{};
};