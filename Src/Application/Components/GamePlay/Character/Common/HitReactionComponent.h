#pragma once
#include <optional>
#include "PostureComponent.h"
#include "HealthComponent.h"
#include "WeaponComponent.h"
#include "ShapeSurfaceQuery.h"
#include "HitResolution.h"
#include "Application/Definitions/Character/Common/CombatData.h"
#include "Application/Core/EventBus/Events/CombatEvents.h"

#include "Framework/Utility/KdUtility.h"

class IHitReactionQuery;

// 被弾の裁定・ダメージ適用・演出を行う。
// 内部は「裁定+適用」と「演出」に分かれ、HitResolvedだけで繋がる。
// 演出側は将来HitFeedbackComponentへ切り出す前提で、裁定側のメンバ(Health/Posture/Query)に触れない。
class HitReactionComponent : public ComponentBase
{
public:
	explicit HitReactionComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override;

	// パリィ/ガード/通常被弾のどれで判定するかを問い合わせる相手を登録する
	void SetQuerySource(IHitReactionQuery* query) { query_ = query; }

	// ブロック/パリィ時のエフェクト発生元として使う自分の武器(演出用)
	void SetWeapon(Handle<WeaponComponent> weapon) { weapon_ = weapon; }

private:
	// ===== 裁定+適用 =====

	// 攻撃側(WeaponComponent)から再通知されるヒット情報の受け口
	void OnHit(const Events::AttackHit& ctx);

	// 受付判定(Query未設定/死亡済み/多段ヒットの重複を弾く)。通れば被弾済みとして記録する。
	bool Accept(const Events::AttackHit& ctx);

	// 自分のHealth/Postureへ適用する。体幹崩壊は暫定でReset。
	ApplyResult Apply(const HitOutcome& outcome);

	// パリィ時の攻撃側への体幹ダメージとAttackDeflected通知
	void NotifyAttacker(const HitOutcome& outcome, const HitResolved& resolved);

	// 結果をQuery(State側)へ通知する。致死ならガード/スタガーへ入らない。
	void Dispatch(const HitOutcome& outcome, const ApplyResult& result);

	// ctxを値の通知データへ詰め替える。ctxを読むのはここだけ。
	HitResolved BuildResolved(const HitOutcome& outcome, const ApplyResult& result, const Events::AttackHit& ctx) const;

	Math::Vector3 ComputeIncomingDirection(const Events::AttackHit& ctx) const;
	Math::Vector3 ComputeDirectionFromAttacker(GameObject* attacker) const;

	IHitReactionQuery* query_ = nullptr;
	PostureComponent* postureComponent_ = nullptr;
	HealthComponent* healthComponent_ = nullptr;
	TransformComponent* transform_ = nullptr;

	ScopedSubscriber subscriber_;

	// ===== 演出(HitResolvedだけを入力にする) =====

	// SE/カメラシェイク/ヒットストップ/エフェクト/歪みの入口
	void PlayFeedback(const HitResolved& r);

	const ReactionEventConfig& SelectReaction(const HitResolved& r) const;

	// 反射エフェクト: 自武器上の最近点から、法線+反射の合成方向へ出す
	void SpawnReflectEffect(const ReactionEventConfig& config, const HitResolved& r);
	// 通常エフェクト: 衝突点から、攻撃側へ向けた法線方向へ出す
	void SpawnNormalEffect(const ReactionEventConfig& config, const HitResolved& r);

	static Math::Vector3 ComputeBodyHitNormal(const HitResolved& r);
	static Math::Vector3 BlendReflectDirection(const Math::Vector3& normal, const Math::Vector3& incoming, float normalBlendRatio);

	// 自武器と攻撃側武器の最近点対。取れなければnullopt。
	std::optional<ShapeSurfaceQuery::ClosestPair> FindWeaponClashPoints(const HitResolved& r) const;

	Handle<WeaponComponent> weapon_;
};
