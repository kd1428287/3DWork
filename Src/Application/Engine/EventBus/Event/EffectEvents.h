#pragma once

#include "Event.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 座標だけで表現できる単純なエフェクトのID
// ※ペイロード(必要なパラメータ)が座標だけで足りるものはここに追加していく。
//   逆に「向きベクトルが要る」等、データの形が違うものは専用のイベント型を作ること。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
enum class EffectId
{
	HitSpark,		// 被弾ヒット
	FootDust,		// 足元の土煙
	BloodSplatter,	// 被ダメージ血飛沫
	// 必要に応じて追加
};

namespace Events
{
	namespace Effect
	{
		// 座標だけで表現できる単純なエフェクトの汎用発生イベント
		// (EffectDispatcher内の対応表(EffectId→発生設定)を引いて処理される)
		struct GenericEffectSpawnEvent : public Event
		{
			EffectId		Id;
			Math::Vector3	Position;
		};

		// 鍔迫り合いの火花(武器vs武器)
		// 両武器の進行方向を持つ点が、座標だけのGenericEffectSpawnEventと異なるため
		// 専用の具象型として分けている
		struct WeaponClashEffectEvent : public Event
		{
			Math::Vector3	Position;			// 衝突位置(近似で良い)
			Math::Vector3	SelfWeaponDir;		// 自分側武器の進行方向(正規化済み想定)
			Math::Vector3	OtherWeaponDir;		// 相手側武器の進行方向(正規化済み想定)
			bool			IsParry = true;		// true:ジャストタイミングの弾き返し／false:通常ガードのブロック
		};
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Publishヘルパー
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// EventBus::Publish<T>のTは呼び出し箇所での静的型で決まる(動的な多態ディスパッチではない)。
// 呼び出し元が基底Event&などを経由してPublishすると型が化けてハンドラに届かなくなるため、
// 具象型を意識せず必ず正しい型のままPublishできるよう、生成～送信をこの関数内に閉じ込める。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
inline void PublishGenericEffect(EventBus& bus, EffectId id, const Math::Vector3& pos)
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