#pragma once

struct CardTypeComponent : public ComponentData<CardTypeComponent>
{
	uint32_t cardId;
	uint32_t payloadSizeID; // 容量（GB/MBなど）
	uint32_t bandwidthCost; // 消費帯域幅
	uint32_t baseDelay;     // 初期ディレイターン数
};