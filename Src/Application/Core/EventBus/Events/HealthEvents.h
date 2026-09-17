#pragma once
#include "Event.h"

class GameObject;

// ============================================================
// HP(HealthComponent)関連のイベント群。
//
// 全てSceneContext::eventBus(シーン全体で共有される、Scene生存中は
// 消えない安全なバス)経由で発行される想定。GameObject単位の
// ローカルEventBus(GameObject::GetLocalEventBus())は使わない。
//
// 理由: EnemyのHPバー等、対象(Player/Enemy)とは別のGameObjectが
// イベントを購読するケースがあり、対象のローカルバスを直接購読すると
// 対象の破棄と同時にバスごと消えてダングリング参照になってしまう。
// シーン全体のバスであれば、対象がどちらのタイミングで破棄されても
// 購読解除(ScopedSubscriber)が常に安全に行える(Handle.hのHandle<T>と
// 同じ「生存期間が結びつかない相手への参照」の考え方)。
// ============================================================

// HPが変化した(ダメージ/回復/SetCurrent/SetMax等)
struct HealthChangedEvent : public Event {
	Handle<GameObject> source;	// 誰のHPが変化したか
	float ratio = 1.0f;		// 変化後の割合(0.0～1.0)
};

// HPが0になった(死亡)
struct HealthDiedEvent : public Event {
	Handle<GameObject> source;
};

inline void PublishHealthChangeEvent(EventBus& bus, Handle<GameObject> obj, float ratio)
{
	HealthChangedEvent e;
	e.source = obj;
	e.ratio = ratio;
	bus.Publish(e);
}
