#pragma once

#include "Application/Editor/EditorHost.h"
#include "Application/main.h"
#include "Application/Editor/Common/EditorViewPort.h"
#include "Application/Core/EventBus/Events/TimeScaleEvents.h"

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
	}

private:
	EventBus& eventBus_;
	uint64_t pauseID_ = 0;
};
