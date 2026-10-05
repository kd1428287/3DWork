#pragma once
#include <optional>
#include <string>

#include "AttackSourceComponent.h"
#include "PostureComponent.h"
#include "HealthComponent.h"
#include "ShapeSurfaceQuery.h"
#include "../../../Physics/Movement/VelocityComponent.h"
#include "../../../Tags/IHitReactionQuery.h"
#include "Application/Core/EventBus/Events/CollisionEvents.h"
#include "Application/Definitions/Prefab/BitFlags.h" 

class WeaponComponent;
struct AttackHitContext;

// ============================================================
// 状態(ヒット/ブロック/パリィ)ごとの反応設定。
// SetConfig()で一括注入し、HitReactionConfigに3状態分を持たせる。
// ============================================================
struct ReactionEventConfig
{
	bool enableCameraShake = false;
	bool enableHitStop = false;
	bool enableEffect = true;

	std::string effectName = "";

	// true: 衝突法線と反射ベクトルを合成した方向で出す(SpawnRefrectEffect)。
	// false: 衝突法線方向で出す(SpawnNormalEffect)。
	bool useReflectDirection = false;

	// 反射エフェクトの合成比率。1.0で法線のみ、0.0で反射のみ。
	float normalBlendRatio = 0.5f;

	float cameraShakeIntensity = 0.0f;
	float hitStopDurationSeconds = 0.0f;
	float knockbackPower = 0.0f;
};

struct HitReactionConfig
{
	// 状態別のリアクション設定
	ReactionEventConfig hit;
	ReactionEventConfig block;
	ReactionEventConfig parry;

	// 共通または特殊な設定
	float hitStopDelaySeconds = 0.0f;
	float largeStaggerDuration = 0.6f;
};

class HitReactionComponent : public ComponentBase
{
public:
	explicit HitReactionComponent(GameObject* owner) : ComponentBase(owner) {}

	using Config = HitReactionConfig;
	void SetConfig(const Config& config) { config_ = config; }

	void Awake() override;

	// パリィ/ガード/通常被弾のどれで判定するかを問い合わせる相手を登録する
	void SetQuerySource(IHitReactionQuery* query) { query_ = query; }

	// ブロック/パリィ時のエフェクト発生元として使う自分の武器
	void SetWeapon(Handle<WeaponComponent> weapon) { weapon_ = weapon; }

private:
	// 攻撃側(WeaponComponent)から再通知されるヒット情報の受け口
	void OnHit(const AttackHitContext& ctx);

	// 状態別の演出(カメラシェイク/ヒットストップ/エフェクト)
	void HitReaction(const ReactionEventConfig& config, const AttackHitContext& ctx);

	// 反射エフェクト: 自武器上の最近点から、法線+反射の合成方向へ出す
	void SpawnRefrectEffect(const ReactionEventConfig& config, const AttackHitContext& ctx);
	// 通常エフェクト: 衝突点から、攻撃側へ向けた法線方向へ出す
	void SpawnNormalEffect(const ReactionEventConfig& config, const AttackHitContext& ctx);

	Math::Vector3 ComputeKnockbackDirection(GameObject* attacker) const;
	Math::Vector3 ComputeIncomingDirection(const AttackHitContext& ctx) const;
	Math::Vector3 ComputeBodyHitNormal(const AttackHitContext& ctx, const Math::Vector3& incoming) const;
	static Math::Vector3 BlendReflectDirection(const Math::Vector3& normal, const Math::Vector3& incoming, float normalBlendRatio);

	// 自武器と攻撃側武器の最近点対。取れなければnullopt。
	std::optional<ShapeSurfaceQuery::ClosestPair> FindWeaponClashPoints(const AttackHitContext& ctx) const;

	IHitReactionQuery* query_ = nullptr;

	PostureComponent* postureComponent_ = nullptr;
	HealthComponent* healthComponent_ = nullptr;
	VelocityComponent* velocityComponent_ = nullptr;
	TransformComponent* transform_ = nullptr;

	Handle<WeaponComponent> weapon_;

	HitReactionConfig config_;

	ScopedSubscriber subscriber_;
};