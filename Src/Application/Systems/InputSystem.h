#pragma once

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
			// エディタ描画のON/OFFを切り替え
			EditorViewport::Instance().ToggleEnabled();
			
			bool flg = EditorViewport::Instance().IsEnabled();
			KdInputManager::Instance().SetAxisConfineToWindowCenter("Look", !flg);
			
			// エディタOFF中(プレイ中)はカーソルを隠し、ON中(編集中)は表示する
			ShowCursor(flg);
			
			if (flg)
			{
				PublishPause(eventBus_, pauseID_);
			} 
			else
			{
				PublishPauseCancel(eventBus_, pauseID_);
			}

		}
	}

private:
	EventBus& eventBus_;
	uint64_t pauseID_ = 0;
};
