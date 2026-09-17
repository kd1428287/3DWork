#pragma once
#include "AttackSourceComponent.h"
#include "PostureComponent.h"
#include "HealthComponent.h"
#include "../../../Physics/Movement/VelocityComponent.h"
#include "../../../Tags/IHitReactionQuery.h"
#include "Application/Core/EventBus/Events/CollisionEvents.h"

class WeaponComponent;

class HitReactionComponent : public ComponentBase
{
public:
	explicit HitReactionComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override;

	// パリィ/ガード/通常被弾のどれで判定するかを問い合わせる相手を登録する
	void SetQuerySource(IHitReactionQuery* query) { query_ = query; }

	// 被弾時、鍔迫り合いの火花エフェクトの発生元として使う自分の武器
	void SetWeapon(Handle<WeaponComponent> weapon) { weapon_ = weapon; }

	// 体幹が壊れた(崩し発生)場合の大きい怯みの秒数
	void SetLargeStaggerDuration(float seconds) { largeStaggerDuration_ = seconds; }

private:
	void OnCollisionEnter(const Events::Collision::CollisionEnterEvent& e);
	void SpawnWeaponClashEffect(GameObject* attackerWeaponObj, bool isParry);
	void SpawnDamageEffect(GameObject* self,GameObject* attackerWeaponObj);

	Math::Vector3 ComputeKnockbackDirection(GameObject* attacker) const;

	IHitReactionQuery* query_ = nullptr;

	PostureComponent* postureComponent_ = nullptr;
	HealthComponent* healthComponent_ = nullptr;
	VelocityComponent* velocityComponent_ = nullptr;
	TransformComponent* transform_ = nullptr;
	
	Handle<WeaponComponent> weapon_;

	static constexpr float kGuardKnockbackPower = 2.0f;

	float largeStaggerDuration_ = 0.6f;

	ScopedSubscriber subscriber_;
};
