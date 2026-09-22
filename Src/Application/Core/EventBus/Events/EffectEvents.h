#pragma once

#include "Event.h"

namespace Events
{
	namespace Effect
	{
		// 座標＋方向で表現できる汎用エフェクトの発生イベント(一撃だけ、その場で発生)
		//	Directionは方向を使わないエフェクト(HitSpark等)では{0,0,0}のまま(≒未指定)でよく、
		//	鍔迫り合いの火花のような「特定方向へ勢いよく飛ぶ」エフェクトもこの1つの型で表現する
		//	(GPUParticleLayer::Shape.DirScale/Offsetがbasedir={0,0,0}でも成立する設計のため)
		struct GenericEffectSpawnEvent : public Event
		{
			std::string		Id;
			Math::Vector3	Position;
			Math::Vector3	Direction;
		};

		// 継続再生(Continuous、または再発生ありのBurst)を、動く発生源に追従させて再生するための3点セット
		// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
		// EffectDispatcherのm_simpleEffects(名前→テンプレートのEffectInstance)はイベントの度に
		// Emit()するだけの「使い回し1体」なので、動く発生源(松明・キャラクター追従の砂煙等)を
		// 複数同時に、それぞれ違う位置で継続再生することはできない。
		//
		// この3イベントは、m_simpleEffectsとは別に「InstanceKeyごとに1体、独立したEffectInstance」を
		// EffectDispatcher::m_activeInstancesに保持させ、Play()〜位置更新〜Stop()の面倒を見る。
		//
		// ・InstanceKey ：発生源側(呼び出し元)が一意に振るID。同じ発生源からは常に同じ値を使う
		//                 (例："Torch_" + std::to_string(オブジェクトのインスタンスID) 等)
		// ・EffectName  ：JSON上のエフェクト名(m_simpleEffectsのキーと同じ、"TorchFire"等)
		//
		// 【使い方】
		//   発生源生成時   ： PublishEffectAttach(bus, key, "TorchFire", pos);
		//   毎フレーム     ： PublishEffectPositionUpdate(bus, key, 現在位置);
		//   発生源消滅時   ： PublishEffectDetach(bus, key);

		// 継続再生の開始：EffectName(m_simpleEffectsのキー)のパラメータを元に、
		// InstanceKey専用の新しいEffectInstanceを生成しPlay()する
		struct EffectAttachSpawnEvent : public Event
		{
			std::string		InstanceKey;
			std::string		EffectName;
			Math::Vector3	Position;
		};

		// 発生源の現在位置を反映させる(発生源が動く場合、呼び出し元は毎フレーム発行する想定)
		struct EffectPositionUpdateEvent : public Event
		{
			std::string		InstanceKey;
			Math::Vector3	Position;
		};

		// 継続再生の終了：新規発生を止め、m_activeInstancesから当該インスタンスを破棄する
		struct EffectDetachEvent : public Event
		{
			std::string		InstanceKey;
		};
	}
}

// Publishヘルパー
inline void PublishGenericEffect(EventBus& bus, const std::string& id, const Math::Vector3& pos, const Math::Vector3& dir = Math::Vector3::Zero)
{
	Events::Effect::GenericEffectSpawnEvent e;
	e.Id = id;
	e.Position = pos;
	e.Direction = dir;
	// 念のため正規化
	e.Direction.Normalize();

	bus.Publish(e);
}

inline void PublishEffectAttach(EventBus& bus, const std::string& instanceKey,
	const std::string& effectName, const Math::Vector3& pos)
{
	Events::Effect::EffectAttachSpawnEvent e;
	e.InstanceKey = instanceKey;
	e.EffectName = effectName;
	e.Position = pos;

	bus.Publish(e);
}

inline void PublishEffectPositionUpdate(EventBus& bus, const std::string& instanceKey, const Math::Vector3& pos)
{
	Events::Effect::EffectPositionUpdateEvent e;
	e.InstanceKey = instanceKey;
	e.Position = pos;

	bus.Publish(e);
}

inline void PublishEffectDetach(EventBus& bus, const std::string& instanceKey)
{
	Events::Effect::EffectDetachEvent e;
	e.InstanceKey = instanceKey;

	bus.Publish(e);
}