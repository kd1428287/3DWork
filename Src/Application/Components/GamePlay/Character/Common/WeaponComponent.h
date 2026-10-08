#pragma once
#include "../../../Physics/Collision/ColliderComponent.h"
#include "../../../Graphics/Effect/SlashTrailComponent.h"
#include "AttackSourceComponent.h"
#include "Application/Core/EventBus/Events/CollisionEvents.h"

struct AttackHitContext : public Event {
	CollisionMath::OverlapResult hitResult;
	Handle<GameObject> attacker;
	Handle<AttackSourceComponent> attackSource;
	Handle<GameObject> weaponObject;
	AttackDamageData damageData;
	Math::Vector3 lastValidSwingDir;
};

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

		EventBus& localBus = GetOwner()->GetLocalEventBus();
		const SubscriptionId subscriptionId = localBus.Subscribe<Events::CollisionEnterEvent>(
			[this](const Events::CollisionEnterEvent& e) { OnCollisionEnter(e); });
		subscriber_ = ScopedSubscriber(&localBus, subscriptionId);
	}

	void Update(float deltaTime) override
	{
		if (deltaTime <= 0.0f)return;

		bufferHead_ = (bufferHead_ + 1) % BUFFER_SIZE;
		posBuffer_[bufferHead_] = transform_->GetPosition();

		constexpr float MIN_DISTANCE_SQ = 0.001f * 0.001f; // 閾値（例: 1mm以上）

		for (size_t i = 1; i < BUFFER_SIZE; ++i) {
			size_t prevIndex = (bufferHead_ + BUFFER_SIZE - i) % BUFFER_SIZE;
			const Math::Vector3& prevPos = posBuffer_[prevIndex];

			Math::Vector3 diff = transform_->GetPosition() - prevPos;

			// 十分な移動距離があれば正規化して軌道を更新
			if (diff.LengthSquared() >= MIN_DISTANCE_SQ) {
				diff.Normalize(); // SimpleMathのNormalizeは自身を正規化する
				lastValidSwingDir_ = diff;
				return;
			}
		}
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
			attackSource_->ClearHitLog();
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

	void OnCollisionEnter(const Events::CollisionEnterEvent& e)
	{
		if (!e.SelfIs(ColliderCategory::HitBox) || !e.OtherIs(ColliderCategory::HurtBox))return;

		AttackHitContext c;
		c.hitResult = e.hitResult;
		c.attacker = attackSource_->GetOwnerHandle();
		c.attackSource = Handle<AttackSourceComponent>(attackSource_);
		c.weaponObject = Handle<GameObject>(GetOwner());
		c.lastValidSwingDir = lastValidSwingDir_;
		c.damageData = attackSource_->GetAttackDamageData();
		e.otherObject->GetLocalEventBus().Publish(c);
	}

	TransformComponent* GetTransform() const { return transform_; }
	ColliderComponent* GetCollider() const { return collider_; }
	AttackSourceComponent* GetAttackSource() const { return attackSource_; }

private:
	TransformComponent* transform_ = nullptr;
	ColliderComponent* collider_ = nullptr;
	AttackSourceComponent* attackSource_ = nullptr;
	SlashTrailComponent* trail_ = nullptr; // 任意

	ScopedSubscriber subscriber_;

	static constexpr size_t BUFFER_SIZE = 5;
	std::array<Math::Vector3, BUFFER_SIZE> posBuffer_;
	size_t bufferHead_ = 0;
	Math::Vector3 lastValidSwingDir_;
	static constexpr float SWING_THRESHOLD = 0.001f;
};