#pragma once

#include "Event.h"
#include "Application/Core/Scene/SceneType.h"

namespace Events
{
	// 指定のシーンへ遷移する汎用イベント
	struct SceneChangeRequest : public Event
	{
		SceneType nextScene;
		bool withTransition;	// falseなら演出なしで即切り替え

		explicit SceneChangeRequest(SceneType scene, bool transition = true)
			: nextScene(scene), withTransition(transition)
		{
		}
	};

	// 現在のシーンを生成し直すイベント
	struct ReloadingScene : public Event
	{
	};
}
