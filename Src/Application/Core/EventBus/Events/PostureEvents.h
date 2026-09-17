#pragma once
#include "Event.h"

class GameObject;

// ============================================================
// 体幹(PostureComponent)関連のイベント。
// HealthEvents.hと同じ理由でSceneContext::eventBus経由で発行する。
// ============================================================

// 体幹値が変化した(被弾/自然回復/リセット時)
struct PostureChangedEvent : public Event {
	Handle<GameObject> source;
	float ratio = 0.0f;
};
