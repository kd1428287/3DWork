#pragma once

#include "Application/Core/EventBus/Events/Event.h"

class GameObject;

namespace Events
{
	// シーン全体のタイムスケール変更リクエスト
	struct ChangeGlobalTimeScale : public Event
	{
		float timeScale = 1.0f;
		float duration = -1.0f;
		mutable uint64_t issuedRequestId = 0; // TimeScaleSystemが発行直後に書き込む
	};

	// 特定のObjectFlagsマスクに一致するオブジェクト群のタイムスケール変更リクエスト
	struct ChangeMaskTimeScale : public Event
	{
		uint8_t mask = 0;
		float timeScale = 1.0f;
		float duration = -1.0f;
	};

	// 特定の1個体に対するタイムスケール変更リクエスト
	struct ChangeObjectTimeScale : public Event
	{
		Handle<GameObject> target;
		float timeScale = 1.0f;
		float duration = -1.0f;
	};

	// 無制限(duration <= 0)で適用したタイムスケール要求のキャンセル
	struct CancelTimeScaleRequest : public Event
	{
		uint64_t requestId = 0;
	};

	// リクエスト受理時に発行元へ払い出されたIDを通知するイベント
	struct TimeScaleRequestIssued : public Event
	{
		uint64_t requestId = 0;
	};

	// --- Helper Functions ---

	inline void PublishHitStop(EventBus& bus, float scale, float duration)
	{
		ChangeGlobalTimeScale e;
		e.timeScale = scale;
		e.duration = duration;
		bus.Publish(e);
	}

	inline void PublishPause(EventBus& bus, uint64_t& outRequestId)
	{
		ChangeGlobalTimeScale e;
		e.timeScale = 0.0f;
		e.duration = -1.0f;
		bus.Publish(e);
		outRequestId = e.issuedRequestId;
	}

	inline void PublishPauseCancel(EventBus& bus, uint64_t requestId)
	{
		CancelTimeScaleRequest e;
		e.requestId = requestId;
		bus.Publish(e);
	}
}