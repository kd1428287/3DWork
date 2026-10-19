#pragma once

#include "Application/Editor/EditorHost.h"
#include "Application/main.h"
#include "Application/Editor/Common/EditorViewPort.h"
#include "Application/Core/EventBus/Events/TimeScaleEvents.h"
#include "Application/Core/EventBus/Events/SceneEvents.h"
#include "Application/Core/EventBus/Events/DebugEvents.h"


class InputSystem
{
public:
	explicit InputSystem(EventBus& bus) :eventBus_(bus), pauseID_(0) {};

	void Update(float deltaTime)
	{
		if (KdInputManager::Instance().IsPress("Editor")) 
		{
			if (EditorHost::Instance().IsEnabled())
			{
				Events::PublishPause(eventBus_, pauseID_);
			} 
			else
			{
				Events::PublishPauseCancel(eventBus_, pauseID_);
			}

		}

		// Debug

		if (KdInputManager::Instance().IsPress("Debug2"))
		{
			GLOBALEVENT->Publish(Events::SceneChangeRequest(SceneType::Title));
		}

		if (KdInputManager::Instance().IsPress("Debug3"))
		{
			GLOBALEVENT->Publish(Events::SceneChangeRequest(SceneType::Game));
		}

		if (KdInputManager::Instance().IsPress("Debug4"))
		{
			GLOBALEVENT->Publish(Events::SceneChangeRequest(SceneType::Result));
		}

		if (KdInputManager::Instance().IsPress("Debug5"))
		{
			GLOBALEVENT->Publish(Events::EnemyKill());
		}

		if (KdInputManager::Instance().IsPress("Debug6"))
		{
			Application::Instance().ShowCursolEnable(true);
			KdInputManager::Instance().SetAxisConfineToWindowCenter("Look", false);
		}

		if (KdInputManager::Instance().IsPress("Debug7"))
		{
			Application::Instance().ShowCursolEnable(false);
			KdInputManager::Instance().SetAxisConfineToWindowCenter("Look", true);
		}
	}

private:
	EventBus& eventBus_;
	uint64_t pauseID_ = 0;
};
