#pragma once
#include "../Engine/EventBus/Event/Event.h"
#include "SceneType.h"

namespace Events
{
	namespace Scene
	{
		// 指定のシーンへ遷移する汎用イベント
		struct SceneChangeRequestEvent : public Event
		{
			SceneType nextScene;

			explicit SceneChangeRequestEvent(SceneType scene)
				: nextScene(scene)
			{}
		};

		// 現在のシーンを生成し直すイベント
		struct ReloadingSceneEvent : public Event
		{
		};
	}
}