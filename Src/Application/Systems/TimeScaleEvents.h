#pragma once

#include "../Engine/EventBus/Event/Event.h"

class GameObject;

// ============================================================
// TimeScaleSystem宛てのイベント群。
// ============================================================
namespace Events
{
	namespace TimeScale
	{
		// ObjectManager全体(=シーン全体)のtimeScaleを操作する。
		struct SetGlobalTimeScaleEvent : public Event
		{
			float timeScale = 1.0f;
			float duration = -1.0f;
		};

		// 指定したObjectFlagsマスクに一致するGameObject群のtimeScaleを一括で操作する
		struct SetMaskTimeScaleEvent : public Event
		{
			uint8_t mask = 0;
			float timeScale = 1.0f;
			float duration = -1.0f;
		};

		// 特定の1個体だけのtimeScaleを操作する。
		struct SetObjectTimeScaleEvent : public Event
		{
			Handle<GameObject> target;
			float timeScale = 1.0f;
			float duration = -1.0f;
		};

		// duration <= 0 (無期限)で登録した要求を、対応するidを指定して明示的に解除するためのイベント
		struct CancelTimeScaleRequestEvent : public Event
		{
			uint64_t requestId = 0;
		};

		// TimeScaleSystemがSet*TimeScaleEventを受理した際、発行元へ
		// 払い出したrequestIdを通知するためのイベント。
		// 無期限の要求(duration<=0)を後から明示的に解除したい発行元だけが
		// これを購読してidを控えておけばよい。
		struct TimeScaleRequestIssuedEvent : public Event
		{
			uint64_t requestId = 0;
		};
	}
}

inline void RequestHitStopEvent(EventBus& bus, float scale, float duration)
{
	Events::TimeScale::SetGlobalTimeScaleEvent e;
	e.timeScale = scale;
	e.duration = duration;
	bus.Publish(e);
}
