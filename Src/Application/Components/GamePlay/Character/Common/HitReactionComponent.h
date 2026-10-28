#pragma once
#include "PostureComponent.h"
#include "HealthComponent.h"
#include "ShapeSurfaceQuery.h"
//#include "../../../Tags/IHitReactionQuery.h"
#include "Application/Definitions/Character/Common/CombatData.h"
#include "Application/Core/EventBus/Events/CombatEvents.h"

#include "Framework/Utility/KdUtility.h"

#include "WeaponComponent.h"


//class WeaponComponent;
class IHitReactionQuery;

// 被弾の判定(ヒット/ブロック/パリィ)とダメージ適用を行う。
// 演出データ(SE/エフェクト等)は攻撃側のAttackVisualData(ctx.visualData)から受け取る。
class HitReactionComponent : public ComponentBase
{
public:
	explicit HitReactionComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override;

	// パリィ/ガード/通常被弾のどれで判定するかを問い合わせる相手を登録する
	void SetQuerySource(IHitReactionQuery* query) { query_ = query; }

	// ブロック/パリィ時のエフェクト発生元として使う自分の武器
	void SetWeapon(Handle<WeaponComponent> weapon) { weapon_ = weapon; }

private:
	// 攻撃側(WeaponComponent)から再通知されるヒット情報の受け口
	void OnHit(const Events::AttackHit& ctx);

	// 状態別の演出(SE/カメラシェイク/ヒットストップ/エフェクト)
	void HitReaction(const ReactionEventConfig& config, const Events::AttackHit& ctx);

	// 反射エフェクト: 自武器上の最近点から、法線+反射の合成方向へ出す
	void SpawnRefrectEffect(const ReactionEventConfig& config, const Events::AttackHit& ctx);
	// 通常エフェクト: 衝突点から、攻撃側へ向けた法線方向へ出す
	void SpawnNormalEffect(const ReactionEventConfig& config, const Events::AttackHit& ctx);

	Math::Vector3 ComputeDirectionFromAttacker(GameObject* attacker) const;
	Math::Vector3 ComputeIncomingDirection(const Events::AttackHit& ctx) const;
	Math::Vector3 ComputeBodyHitNormal(const Events::AttackHit& ctx, const Math::Vector3& incoming) const;
	static Math::Vector3 BlendReflectDirection(const Math::Vector3& normal, const Math::Vector3& incoming, float normalBlendRatio);

	// 自武器と攻撃側武器の最近点対。取れなければnullopt。
	std::optional<ShapeSurfaceQuery::ClosestPair> FindWeaponClashPoints(const Events::AttackHit& ctx) const;

	IHitReactionQuery* query_ = nullptr;

	PostureComponent* postureComponent_ = nullptr;
	HealthComponent* healthComponent_ = nullptr;
	TransformComponent* transform_ = nullptr;

	Handle<WeaponComponent> weapon_;

	ScopedSubscriber subscriber_;
};
