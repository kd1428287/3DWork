#pragma once
#include "../../../Physics/Collision/ColliderComponent.h"
#include "../../../Graphics/Effect/SlashTrailComponent.h"
#include "AttackSourceComponent.h"

// ============================================================
// WeaponComponent
// 「攻撃判定を持つ部位」1つ分の制御をカプセル化する汎用コンポーネント
// ============================================================
class WeaponComponent : public ComponentBase
{
public:
	explicit WeaponComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		collider_ = GetOwner()->GetComponent<ColliderComponent>();
		attackSource_ = GetOwner()->GetComponent<AttackSourceComponent>();
		trail_ = GetOwner()->GetComponent<SlashTrailComponent>(); // 無い部位(素手等)もあるためnullptr許容
	}

	// 攻撃判定発生窓の開閉。有効化のタイミングでalreadyHitをクリアするのは
	// 元のPlayerStatusController::SetWeaponHitBoxEnabled()と同じ理由
	// (1回の判定窓で同じ相手に複数回ヒットしないようにするため)。
	void SetHitBoxEnabled(bool enabled)
	{
		if (collider_ != nullptr) {
			collider_->SetShapeEnabled("HitBox", enabled);
		}
		if (enabled && attackSource_ != nullptr) {
			attackSource_->alreadyHit.clear();
		}
	}

	// 武器の軌跡エフェクトの発生/停止。Trailを持たない部位(拳・足等)では
	// trail_がnullptrのまま何もしない。
	void SetTrailEmitting(bool emitting)
	{
		if (trail_ == nullptr) return;
		if (emitting) trail_->StartEmit();
		else trail_->StopEmit();
	}

	void SetAttackDamageData(AttackDamageData info) { attackSource_->SetAttackDamageData(info); }

	TransformComponent* GetTransform() const { return transform_; }
	ColliderComponent* GetCollider() const { return collider_; }
	AttackSourceComponent* GetAttackSource() const { return attackSource_; }

private:
	TransformComponent* transform_ = nullptr;
	ColliderComponent* collider_ = nullptr;
	AttackSourceComponent* attackSource_ = nullptr;
	SlashTrailComponent* trail_ = nullptr; // 任意
};