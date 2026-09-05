#pragma once
#include "../Engine/EventBus/Event/Event.h"
#include "SceneType.h"

namespace Events
{
	namespace Scene
	{
		// 「指定のシーンへ遷移してほしい」という、発行者を問わない汎用イベント。
		struct SceneChangeRequestEvent : public Event
		{
			SceneType nextScene;

			explicit SceneChangeRequestEvent(SceneType scene)
				: nextScene(scene)
			{}
		};
	}
}