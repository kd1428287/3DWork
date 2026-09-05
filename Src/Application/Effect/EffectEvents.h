#pragma once

#include "../Engine/EventBus/Event/Event.h"

namespace Events
{
	namespace Effect
	{
		// 座標だけで表現できる単純なエフェクトの汎用発生イベント(一撃だけ、その場で発生)
		struct GenericEffectSpawnEvent : public Event
		{
			std::string		Id;
			Math::Vector3	Position;
		};

		// 鍔迫り合いの火花(武器vs武器)
		struct WeaponClashEffectEvent : public Event
		{
			Math::Vector3	Position;			// 衝突位置(近似で良い)
			Math::Vector3	SelfWeaponDir;		// 自分側武器の進行方向(正規化済み想定)
			Math::Vector3	OtherWeaponDir;		// 相手側武器の進行方向(正規化済み想定)
			bool			IsParry = true;		// true:ジャストタイミングの弾き返し／false:通常ガードのブロック
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
inline void PublishGenericEffect(EventBus& bus, const std::string& id, const Math::Vector3& pos)
{
	Events::Effect::GenericEffectSpawnEvent e;
	e.Id = id;
	e.Position = pos;

	bus.Publish(e);
}

inline void PublishWeaponClashEffect(EventBus& bus, const Math::Vector3& pos,
	const Math::Vector3& selfWeaponDir, const Math::Vector3& otherWeaponDir, bool isParry = true)
{
	Events::Effect::WeaponClashEffectEvent e;
	e.Position = pos;
	e.SelfWeaponDir = selfWeaponDir;
	e.OtherWeaponDir = otherWeaponDir;
	e.IsParry = isParry;

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