#include "HitReactionComponent.h"
#include "../Common/CharacterEvents.h"
#include "WeaponComponent.h"
#include <algorithm>
#include "ShapeSurfaceQuery.h"

namespace
{
	// 武器の判定形状名(WeaponComponent::SetHitBoxEnabledと同じ)。
	constexpr const char* kWeaponShapeName = "HitBox";
}

void HitReactionComponent::Awake()
{
	postureComponent_ = GetOwner()->GetComponent<PostureComponent>();
	healthComponent_ = GetOwner()->GetComponent<HealthComponent>();
	velocityComponent_ = GetOwner()->GetComponent<VelocityComponent>();
	transform_ = GetOwner()->GetComponent<TransformComponent>();

	EventBus& localBus = GetOwner()->GetLocalEventBus();
	const SubscriptionId subscriptionId = localBus.Subscribe<AttackHitContext>(
		[this](const AttackHitContext& e) { OnHit(e); });
	subscriber_ = ScopedSubscriber(&localBus, subscriptionId);
}

Math::Vector3 HitReactionComponent::ComputeKnockbackDirection(GameObject* attacker) const
{
	Math::Vector3 dir = (transform_ != nullptr) ? transform_->GetForward() : Math::Vector3::Forward;

	if (attacker == nullptr || transform_ == nullptr) return dir;

	if (TransformComponent* attackerTransform = attacker->GetComponent<TransformComponent>()) {
		dir = transform_->GetPosition() - attackerTransform->GetPosition();
		dir.y = 0.0f;
		if (dir.LengthSquared() < 1e-6f) {
			dir = transform_->GetForward();
		}
		else {
			dir.Normalize();
		}
	}
	return dir;
}

Math::Vector3 HitReactionComponent::ComputeIncomingDirection(const AttackHitContext& ctx) const
{
	// 武器の振り方向。取れなければ攻撃者→自分。
	Math::Vector3 incoming = ctx.lastValidSwingDir;
	if (incoming.LengthSquared() < 1e-6f) {
		return ComputeKnockbackDirection(ctx.attacker.Resolve());
	}
	incoming.Normalize();
	return incoming;
}

Math::Vector3 HitReactionComponent::ComputeBodyHitNormal(
	const AttackHitContext& ctx, const Math::Vector3& incoming) const
{
	// hitNormalは正規化済み。ゼロ(未設定)の場合のみ上向きで代用。
	Math::Vector3 normal = ctx.hitResult.hitNormal;
	if (normal.LengthSquared() < 1e-6f) {
		normal = Math::Vector3(0.0f, 1.0f, 0.0f);
	}

	// 法線を攻撃側へ向け、hitNormalの符号規約に依存しないようにする。
	if (incoming.Dot(normal) > 0.0f) {
		normal = -normal;
	}
	return normal;
}

Math::Vector3 HitReactionComponent::BlendReflectDirection(
	const Math::Vector3& normal, const Math::Vector3& incoming, float normalBlendRatio)
{
	// 反射ベクトル r = d - 2(d·n)n を法線と合成する。
	const Math::Vector3 reflect = incoming - normal * (2.0f * incoming.Dot(normal));
	const float t = std::clamp(normalBlendRatio, 0.0f, 1.0f);

	Math::Vector3 dir = normal * t + reflect * (1.0f - t);
	if (dir.LengthSquared() < 1e-6f) return normal;
	dir.Normalize();
	return dir;
}

std::optional<ShapeSurfaceQuery::ClosestPair> HitReactionComponent::FindWeaponClashPoints(
	const AttackHitContext& ctx) const
{
	GameObject* weaponObject = ctx.weaponObject.Resolve();
	if (weaponObject == nullptr) return std::nullopt;

	WeaponComponent* myWeapon = weapon_.Resolve();
	if (myWeapon == nullptr) return std::nullopt;

	ColliderComponent* myCollider = myWeapon->GetCollider();
	ColliderComponent* attackerCollider = weaponObject->GetComponent<ColliderComponent>();
	if (myCollider == nullptr || attackerCollider == nullptr) return std::nullopt;

	const CollisionShapeEntry* myShape = myCollider->FindShape(kWeaponShapeName);
	const CollisionShapeEntry* attackerShape = attackerCollider->FindShape(kWeaponShapeName);
	if (myShape == nullptr || attackerShape == nullptr) return std::nullopt;

	// 自武器上の、攻撃側武器に最も近い点(onA)と、その対応点(onB)。
	return ShapeSurfaceQuery::ClosestPointsBetweenShapes(*myCollider, *myShape, *attackerCollider, *attackerShape);
}

void HitReactionComponent::SpawnRefrectEffect(const ReactionEventConfig& config, const AttackHitContext& ctx)
{
	if (config.effectName.empty()) return;

	const Math::Vector3 incoming = ComputeIncomingDirection(ctx);

	// 既定は衝突点と胴体法線。武器情報が取れたら上書きする。
	Math::Vector3 pos = ctx.hitResult.hitPos;
	Math::Vector3 normal = ComputeBodyHitNormal(ctx, incoming);

	if (const auto clash = FindWeaponClashPoints(ctx)) {
		pos = clash->onA;

		// 武器同士が離れていれば、自武器→攻撃側の向きを法線にする。
		Math::Vector3 toAttacker = clash->onB - clash->onA;
		if (toAttacker.LengthSquared() > 1e-6f) {
			toAttacker.Normalize();
			normal = toAttacker;
		}
	}
	else if (WeaponComponent* myWeapon = weapon_.Resolve()) {
		if (TransformComponent* weaponTf = myWeapon->GetTransform()) {
			pos = weaponTf->GetPosition();
		}
	}

	const Math::Vector3 dir = BlendReflectDirection(normal, incoming, config.normalBlendRatio);
	PublishGenericEffect(GetOwner()->GetSceneEventBus(), config.effectName, pos, dir);
}

void HitReactionComponent::SpawnNormalEffect(const ReactionEventConfig& config, const AttackHitContext& ctx)
{
	if (config.effectName.empty()) return;

	// 衝突点から、攻撃側へ向けた法線方向へ出す。
	const Math::Vector3 incoming = ComputeIncomingDirection(ctx);
	const Math::Vector3 normal = ComputeBodyHitNormal(ctx, incoming);
	PublishGenericEffect(GetOwner()->GetSceneEventBus(), config.effectName, ctx.hitResult.hitPos, normal);
}

void HitReactionComponent::HitReaction(const ReactionEventConfig& config, const AttackHitContext& ctx)
{
	EventBus& bus = *GetOwner()->GetContext()->eventBus;

	if (config.enableCameraShake) {
		PublishCameraShake(bus, config.cameraShakeIntensity);
	}
	if (config.enableHitStop) {
		PublishHitStop(bus, config_.hitStopDelaySeconds, config.hitStopDurationSeconds);
	}
	if (config.enableEffect) {
		// 反射を使う設定なら反射エフェクト、そうでなければ通常エフェクト。
		if (config.useReflectDirection) SpawnRefrectEffect(config, ctx);
		else                            SpawnNormalEffect(config, ctx);
	}
	if (config.enableDistortion)
	{
		KdShaderManager::Instance().m_postProcessShader.AddShockwaveWorld(ctx.hitResult.hitPos, 0.45f, 0.7f, 0.03f);
	}
}

void HitReactionComponent::OnHit(const AttackHitContext& ctx)
{
	if (query_ == nullptr) return;

	AttackSourceComponent* attackSource = ctx.attackSource.Resolve();
	if (attackSource == nullptr) return;
	if (attackSource->IsAlreadyHit(Handle<GameObject>(GetOwner()))) return;
	attackSource->Hit(Handle<GameObject>(GetOwner()));

	GameObject* attacker = ctx.attacker.Resolve();
	const AttackDamageData& attackData = ctx.damageData;

	if (query_->IsInParryWindow()) {
		// パリィ成立: 攻撃側の体幹を削り、パリィ通知を送る。
		if (attacker != nullptr) {
			if (PostureComponent* attackerPosture = attacker->GetComponent<PostureComponent>()) {
				attackerPosture->AddPostureDamage(attackData.parryPostureDamage);
			}
			attacker->GetLocalEventBus().Publish(AttackSourceComponent::ParriedEvent{});
		}
		HitReaction(config_.parry, ctx);

		query_->NotifyParrySuccess();
	}
	else if (query_->IsGuarding()) {
		// ブロック: 体幹ダメージとHPのチップダメージ、ノックバック。
		HitReaction(config_.block, ctx);

		if (postureComponent_ != nullptr) {
			postureComponent_->AddPostureDamage(attackData.postureDamage);
			if (postureComponent_->IsBroken()) {
				// TODO: 崩し状態(専用State)への遷移は別途実装。
			}
		}
		if (healthComponent_ != nullptr) {
			healthComponent_->TakeDamage(attackData.damage * attackData.chipDamageRatio);
		}
		if (velocityComponent_ != nullptr) {
			velocityComponent_->AddImpulse(ComputeKnockbackDirection(attacker) * config_.block.knockbackPower);
		}

		query_->NotifyGuardHit();
	}
	else {
		// 通常被弾: ダメージ・ノックバック・体幹蓄積、体幹崩壊で大スタン。
		if (healthComponent_ != nullptr) {
			healthComponent_->TakeDamage(attackData.damage);
		}
		if (velocityComponent_ != nullptr) {
			velocityComponent_->AddImpulse(ComputeKnockbackDirection(attacker) * attackData.knockbackPower);
		}

		bool postureBroken = false;
		if (postureComponent_ != nullptr) {
			postureComponent_->AddPostureDamage(attackData.postureDamage);
			postureBroken = postureComponent_->IsBroken();
			if (postureBroken) {
				postureComponent_->Reset();
			}
		}

		HitReaction(config_.hit, ctx);

		query_->EnterStagger(postureBroken, postureBroken ? config_.largeStaggerDuration : attackData.hitStunSeconds);
	}
}