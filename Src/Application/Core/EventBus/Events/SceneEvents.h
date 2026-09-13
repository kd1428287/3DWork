#pragma once
#include "Event.h"
#include "Application/Core/Scene/SceneType.h"

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