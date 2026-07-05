#pragma once

class Event {
public:
	virtual ~Event() = default;
};

namespace Events
{
	namespace Card
	{
		// カードゲーム関連の代表的なイベント。
		// これらの型ごとにLocalEventBus::Subscribe<T>/Publish<T>する。
		struct CardDrawnEvent : public Event{
			GameObject* card;
			int ownerPlayerId;
		};

		struct CardPlayedEvent : public Event {
			GameObject* card;
			int ownerPlayerId;
		};

		struct CardSelectedEvent : public Event {
			GameObject* card;    // 新しく選択されたカード
			GameObject* previous; // 直前まで選択されていたカード(無ければnullptr)
		};
	}
}