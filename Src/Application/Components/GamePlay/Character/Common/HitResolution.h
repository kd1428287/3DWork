#pragma once
#include "Application/Definitions/Character/Common/CombatData.h"
#include "Application/Core/EventBus/Events/CombatEvents.h"

// 被弾の裁定・適用・演出の間で受け渡すデータと、副作用のない裁定関数。
// コンポーネントやQueryには依存しない。

enum class HitType { Hit, Block, Parry };

// 裁定の入力。Queryへの問い合わせ結果を値にしたもの。
struct HitJudgeState
{
	bool inParryWindow = false;
	bool guarding = false;
};

// 裁定の出力。倍率計算まで済んだ最終値。
struct HitOutcome
{
	HitType type = HitType::Hit;
	float healthDamage = 0.0f;
	float postureDamage = 0.0f;
	float attackerPostureDamage = 0.0f; // パリィ時のみ使う
};

// 適用の結果。後続の通知の分岐に使う。
struct ApplyResult
{
	bool died = false;
	bool postureBroken = false;
};

// 裁定結果の通知データ。演出はこれだけを入力にする(将来のイベント化に備え全て値)。
struct HitResolved
{
	HitType type = HitType::Hit;
	float healthDamage = 0.0f;
	float postureDamage = 0.0f;
	bool died = false;
	bool postureBroken = false;

	Math::Vector3 hitPos;
	Math::Vector3 hitNormal;  // 衝突法線(未加工)
	Math::Vector3 incomingDir; // 入射方向(正規化済み)

	AttackVisualData visual;
	Handle<GameObject> attacker;
	Handle<GameObject> weaponObject;
};

// パリィ/ブロック/通常被弾の裁定。
inline HitOutcome ResolveHit(const HitJudgeState& state, const AttackDamageData& data)
{
	HitOutcome outcome;

	if (state.inParryWindow && data.isParryable) {
		outcome.type = HitType::Parry;
		outcome.attackerPostureDamage = data.parryPostureDamage;
		return outcome;
	}

	// パリィ不可の攻撃は猶予中でもブロックへ落ちる。
	if (state.inParryWindow || state.guarding) {
		outcome.type = HitType::Block;
		outcome.healthDamage = data.damage * data.guardDamageRatio;
		outcome.postureDamage = data.postureDamage * data.guardPostureDamageRatio;
		return outcome;
	}

	outcome.type = HitType::Hit;
	outcome.healthDamage = data.damage;
	outcome.postureDamage = data.postureDamage;
	return outcome;
}
