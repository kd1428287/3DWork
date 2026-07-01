#pragma once

// 現在のディレイ状態を管理
struct CardLatencyComponent : public ComponentData<CardLatencyComponent>
{
	uint32_t remainingTurns; // 着信までの残りターン数
};