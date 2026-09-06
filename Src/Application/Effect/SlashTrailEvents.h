#pragma once

#include "../Engine/EventBus/Event/Event.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル(斬撃の軌跡)の記録開始/位置更新/記録終了を表す3点セット
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// GPUパーティクルのEffectAttachSpawnEvent〜EffectDetachEventと同じ構造の使い方をする。
// 剣(武器)側のコンポーネントはSlashTrailDispatcherの存在を一切知らず、
// これらのイベントをEventBusへ発行するだけで良い。
//
// ・InstanceKey：発生源側(呼び出し元)が一意に振るID。同じ剣からは常に同じ値を使う
//                (例："Sword_" + プレイヤーのインスタンスID等)
// ・TrailName  ：SlashTrailDispatcher::RegisterDefinition()で登録した定義名
//                (どのSlashTrailParamsを使うか)
//
// 【使い方】
//   攻撃開始時 ： PublishSlashTrailBegin(bus, "Sword_Player", "PlayerSwordTrail");
//   毎フレーム ： PublishSlashTrailPositionUpdate(bus, "Sword_Player", tipPos, basePos);
//   攻撃終了時 ： PublishSlashTrailEnd(bus, "Sword_Player");
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

namespace Events
{
	namespace SlashTrail
	{
		// 記録開始：TrailName(SlashTrailDispatcher::RegisterDefinition()で登録した定義名)を元に
		// InstanceKey専用の新しいSlashTrailInstanceが生成され、記録が始まる
		struct SlashTrailBeginEvent : public Event
		{
			std::string	InstanceKey;
			std::string	TrailName;
		};

		// 記録中、毎フレーム剣のTip/Base座標を送る
		//	記録中でないInstanceKey(Begin前・End後)に送っても無視される
		struct SlashTrailPositionUpdateEvent : public Event
		{
			std::string		InstanceKey;
			Math::Vector3	Tip;
			Math::Vector3	Base;
		};

		// 記録終了：新規サンプルの記録を止める。
		//	※即座には破棄されない。既存サンプルがフェードアウトし切ってから
		//	  SlashTrailDispatcher::Update()内で自動的に破棄される
		struct SlashTrailEndEvent : public Event
		{
			std::string	InstanceKey;
		};
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Publishヘルパー(EffectEvents.hのPublishGenericEffect等と同じ理由で用意している：
// EventBus::Publish<T>のTは静的型で決まる為、生成～送信をここに閉じ込める)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
inline void PublishSlashTrailBegin(EventBus& bus, const std::string& instanceKey, const std::string& trailName)
{
	Events::SlashTrail::SlashTrailBeginEvent e;
	e.InstanceKey = instanceKey;
	e.TrailName = trailName;

	bus.Publish(e);
}

inline void PublishSlashTrailPositionUpdate(EventBus& bus, const std::string& instanceKey,
	const Math::Vector3& tip, const Math::Vector3& base)
{
	Events::SlashTrail::SlashTrailPositionUpdateEvent e;
	e.InstanceKey = instanceKey;
	e.Tip = tip;
	e.Base = base;

	bus.Publish(e);
}

inline void PublishSlashTrailEnd(EventBus& bus, const std::string& instanceKey)
{
	Events::SlashTrail::SlashTrailEndEvent e;
	e.InstanceKey = instanceKey;

	bus.Publish(e);
}
