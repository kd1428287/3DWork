#pragma once

#include "Event.h"

namespace Events
{
	// 画面遷移の開始要求(TransitionControllerが受け取って駆動する)
	struct TransitionRequest : public Event
	{
		float outTime = 0.6f;
		float inTime = 0.6f;
		Math::Vector2 center = { 0.5f, 0.5f };	// 波紋の中心(0〜1の画面座標)
		Math::Vector3 color = { 0.0f, 0.0f, 0.0f };	// 覆う色
	};

	// 画面が完全に覆われた(この通知でシーン切り替え等を行う)
	struct TransitionCovered : public Event
	{
	};

	// 遷移が終了した
	struct TransitionFinished : public Event
	{
	};
}
