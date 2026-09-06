#pragma once
#include "IHitReactionQuery.h"
#include "../../../Core/Handle.h"
#include "../../Collision/ColliderComponent.h"
#include "../../Collision/AttackSourceComponent.h"
#include "../../../Systems/Collision/CollisionSystem.h"
#include "../Data/PostureComponent.h"
#include "../Data/HealthComponent.h"
#include "../../Movement/VelocityComponent.h"
#include "../../Transform/TransformComponent.h"

class HitReactionComponent : public ComponentBase
{
public:
	explicit HitReactionComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override;
	void Start() override;

	// パリィ/ガード/通常被弾のどれで判定するかを問い合わせる相手を登録する
	void SetQuerySource(IHitReactionQuery* query) { query_ = query; }

	// 被弾時、鍔迫り合いの火花エフェクトの発生元として使う自分の武器
	void SetWeaponCollider(Handle<ColliderComponent> weaponCollider) { weaponCollider_ = weaponCollider; }

	// 体幹が壊れた(崩し発生)場合の大きい怯みの秒数
	void SetLargeStaggerDuration(float seconds) { largeStaggerDuration_ = seconds; }

private:
	void OnCollisionEnter(const CollisionSystem::CollisionEnterEvent& e);
	void SpawnWeaponClashEffect(GameObject* attackerWeaponObj, bool isParry);

	Math::Vector3 ComputeKnockbackDirection(GameObject* attacker) const;

	IHitReactionQuery* query_ = nullptr;

	PostureComponent* postureComponent_ = nullptr;
	HealthComponent* healthComponent_ = nullptr;
	VelocityComponent* velocityComponent_ = nullptr;
	TransformComponent* transform_ = nullptr;

	// 鍔迫り合いエフェクト発生元として使う、自分の武器への弱参照。
	Handle<ColliderComponent> weaponCollider_;

	// ガード時被弾のノックバック強度(元のPlayerStatusControllerの
	// 決め打ち値2.0fをそのまま踏襲)。
	static constexpr float kGuardKnockbackPower = 2.0f;

	float largeStaggerDuration_ = 0.6f;

	ScopedSubscriber subscriber_;
};
