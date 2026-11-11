#pragma once

#include "Event.h"
#include <string>

namespace Events
{
	// 基準点(回転+平行移動の行列)へ一枚テクスチャエフェクトを取り付け、再生を始める。
	// 以降はShapeEffectBaseUpdateで基準点を更新する(固定したい場合は更新しなければよい)。
	// 動きの最後まで再生したら自動で破棄される。途中キャンセルはShapeEffectDetachを使う。
	struct ShapeEffectAttach : public Event
	{
		std::string   InstanceKey;
		std::string   EffectName;
		Math::Matrix  Base;
		float         MaxDuration = 0.0f; // >0: この秒数が過ぎたら自動で破棄(持ち主のDetach漏れ対策)
	};

	struct ShapeEffectBaseUpdate : public Event
	{
		std::string   InstanceKey;
		Math::Matrix  Base;
	};

	struct ShapeEffectDetach : public Event
	{
		std::string InstanceKey;
	};

	// Publishヘルパー
	inline void PublishShapeEffectAttach(EventBus& bus, const std::string& instanceKey,
		const std::string& effectName, const Math::Matrix& base, float maxDuration = 0.0f)
	{
		ShapeEffectAttach e;
		e.InstanceKey = instanceKey;
		e.EffectName = effectName;
		e.Base = base;
		e.MaxDuration = maxDuration;

		bus.Publish(e);
	}

	inline void PublishShapeEffectBaseUpdate(EventBus& bus, const std::string& instanceKey, const Math::Matrix& base)
	{
		ShapeEffectBaseUpdate e;
		e.InstanceKey = instanceKey;
		e.Base = base;

		bus.Publish(e);
	}

	inline void PublishShapeEffectDetach(EventBus& bus, const std::string& instanceKey)
	{
		ShapeEffectDetach e;
		e.InstanceKey = instanceKey;

		bus.Publish(e);
	}
}
