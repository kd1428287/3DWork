#pragma once
#include <cfloat>
#include <memory>
#include <string>
#include "EnemyAIData.h"
#include "IEnemyBehavior.h"
#include "../BehaviorTree/BTWeightedAttackAction.h"
#include "../BehaviorTree/IBTNode.h"
#include "../BehaviorTree/BTNodeStatus.h"
#include "../BehaviorTree/BTComposite.h"
#include "../BehaviorTree/BTCondition.h"
#include "../BehaviorTree/BTOneShotAnimationAction.h"
#include "../Player/PlayerStatusController.h"
#include "../../Transform/TransformComponent.h"
#include "../../Movement/MovementComponent.h"
#include "../../Movement/IMovementSource.h"
#include "../../Movement/FacingDirectionComponent.h"
#include "../../Animation/ModelAnimatorComponent.h"
#include "../../Collision/ColliderComponent.h"
#include "../../Collision/AttackSourceComponent.h"
#include "../../../Systems/Collision/CollisionSystem.h"
#include "../Data/PostureComponent.h"
#include "../Data/HealthComponent.h"
#include "../../../Core/Handle.h"

class EnemyAIController : public ComponentBase, public IMovementSource
{
public:
	EnemyAIController(GameObject* owner, const EnemyAIData& data, std::unique_ptr<IEnemyBehavior> behavior)
		: ComponentBase(owner), data_(data), behavior_(std::move(behavior)) {}

	void Start() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		movementComponent_ = GetOwner()->GetComponent<MovementComponent>();
		modelAnimatorComponent_ = GetOwner()->GetComponent<ModelAnimatorComponent>();
		postureComponent_ = GetOwner()->GetComponent<PostureComponent>();
		healthComponent_ = GetOwner()->GetComponent<HealthComponent>();
		// 無くてもよい(任意)。存在する場合のみルートモーション中の
		// 向き自動追従の一時停止に使う(クラス冒頭コメント参照)。
		facingDirectionComponent_ = GetOwner()->GetComponent<FacingDirectionComponent>();

		if (movementComponent_ != nullptr) {
			movementComponent_->SetMovementSource(this);
		}

		EventBus& localBus = GetOwner()->GetLocalEventBus();

		const SubscriptionId collisionId = localBus.Subscribe<CollisionSystem::CollisionEnterEvent>(
			[this](const CollisionSystem::CollisionEnterEvent& e) { OnCollisionEnter(e); });
		collisionSubscriber_ = ScopedSubscriber(&localBus, collisionId);

		if (healthComponent_ != nullptr) {
			const SubscriptionId diedId = localBus.Subscribe<HealthComponent::DiedEvent>(
				[this](const HealthComponent::DiedEvent&) { OnDied(); });
			diedSubscriber_ = ScopedSubscriber(&localBus, diedId);
		}

		const SubscriptionId parriedId = localBus.Subscribe<AttackSourceComponent::ParriedEvent>(
			[this](const AttackSourceComponent::ParriedEvent& e) { OnParried(e); });
		parriedSubscriber_ = ScopedSubscriber(&localBus, parriedId);

		root_ = behavior_->BuildTree(this);
		behavior_->OnSpawned(this);
	}

	void Update(float deltaTime) override
	{
		if (isDead_) {
			despawnTimer_ -= deltaTime;
			if (despawnTimer_ <= 0.0f) {
				RequestDespawn();
			}
			return;
		}

		if (attackCooldownTimer_ > 0.0f) {
			attackCooldownTimer_ -= deltaTime;
			if (attackCooldownTimer_ < 0.0f) attackCooldownTimer_ = 0.0f;
		}

		UpdateTargetAcquisition();
		if (root_ != nullptr) root_->Tick(this, deltaTime);

		ApplyRootMotion();
	}

	// --- IMovementSourceの実装 ---------------------------------------------
	Math::Vector3 GetDesiredVelocity() override { return desiredVelocity_; }

	// --- 実行層API(各Behaviorが組み立てるActionノードから呼ばれる) --------
	const EnemyAIData& GetData() const { return data_; }

	Math::Vector3 GetPosition() const {
		return transform_ != nullptr ? transform_->GetPosition() : Math::Vector3::Zero;
	}

	// 索敵のヒステリシス込みの「今ターゲットを捕捉しているか」。
	// UpdateTargetAcquisition()が毎フレーム更新する。
	bool HasTarget() const { return hasTarget_; }

	float DistanceToTarget() const {
		if (!hasTarget_ || targetTransform_ == nullptr || transform_ == nullptr) return FLT_MAX;
		return (targetTransform_->GetPosition() - transform_->GetPosition()).Length();
	}

	bool IsTargetInAttackRange() const {
		if (!HasTarget()) return false;
		const float dist = DistanceToTarget();
		for (const auto& atk : data_.attacks) {
			if (dist <= atk.maxRange) return true;
		}
		return false;
	}

	const EnemyAttackDefinition* ChooseAttack() const;

	bool IsAttackOnCooldown() const { return attackCooldownTimer_ > 0.0f; }
	void NotifyAttackCompleted() { attackCooldownTimer_ = data_.attackIntervalDuration; }

	Math::Vector3 GetCurrentPatrolPoint() const {
		if (data_.patrolPoints.empty()) return GetPosition();
		return data_.patrolPoints[patrolIndex_ % data_.patrolPoints.size()];
	}
	void AdvanceToNextPatrolPoint() {
		if (!data_.patrolPoints.empty()) {
			patrolIndex_ = (patrolIndex_ + 1) % data_.patrolPoints.size();
		}
	}

	Math::Vector3 GetTargetPositionOrSelf() const {
		if (hasTarget_ && targetTransform_ != nullptr) return targetTransform_->GetPosition();
		return GetPosition();
	}

	void SetDesiredVelocity(const Math::Vector3& velocity) { desiredVelocity_ = velocity; }
	void StopMovement() { desiredVelocity_ = Math::Vector3::Zero; }

	void FaceHorizontalTarget(const Math::Vector3& targetPosition);

	// 同じループアニメーションを毎フレーム再生し直さないための薄いラッパー。
	void PlayAnimationIfChanged(const std::string& name, bool loop) {
		if (name == currentAnimationName_) return;
		PlayAnimation(name, loop);
	}

	void PlayAnimation(const std::string& name, bool loop = false, float targetDurationSeconds = -1.0f, bool useRootMotion = false) {
		currentAnimationName_ = name;
		if (modelAnimatorComponent_ != nullptr) {
			modelAnimatorComponent_->SetRootMotionBoneName(useRootMotion ? kRootMotionBoneName : "");
			modelAnimatorComponent_->Play(name, loop, targetDurationSeconds);
		}
		if (facingDirectionComponent_ != nullptr) {
			facingDirectionComponent_->SetUpdateEnabled(!useRootMotion);
		}
	}

	// --- 武器の攻撃判定 --------------------------------------------------
	void SetWeapon(Handle<ColliderComponent> weaponCollider, Handle<AttackSourceComponent> weaponAttackSource) {
		weaponCollider_ = weaponCollider;
		weaponAttackSource_ = weaponAttackSource;
	}

	void SetWeaponHitBoxEnabled(bool enabled) {
		if (ColliderComponent* collider = weaponCollider_.Resolve()) {
			collider->SetShapeEnabled("HitBox", enabled);
		}
		if (enabled) {
			if (AttackSourceComponent* source = weaponAttackSource_.Resolve()) {
				source->alreadyHit.clear();
			}
		}
	}

	// --- 体幹 --------------------------------------------------------------
	PostureComponent* GetPostureComponent() const { return postureComponent_; }

	// --- 死亡時の道連れ破棄 -------------------------------------------------
	void RegisterOwnedObject(Handle<GameObject> obj) {
		ownedObjects_.push_back(obj);
	}

private:
	void UpdateTargetAcquisition();
	TransformComponent* FindPlayerTransform() const;
	void OnCollisionEnter(const CollisionSystem::CollisionEnterEvent& e);
	void OnParried(const AttackSourceComponent::ParriedEvent& e);
	void OnDied();
	void RequestDespawn();
	void ApplyRootMotion();

	EnemyAIData data_;
	std::unique_ptr<IEnemyBehavior> behavior_;

	TransformComponent* transform_ = nullptr;
	MovementComponent* movementComponent_ = nullptr;
	ModelAnimatorComponent* modelAnimatorComponent_ = nullptr;
	PostureComponent* postureComponent_ = nullptr;
	HealthComponent* healthComponent_ = nullptr;
	FacingDirectionComponent* facingDirectionComponent_ = nullptr;

	Handle<ColliderComponent> weaponCollider_;
	Handle<AttackSourceComponent> weaponAttackSource_;
	std::vector<Handle<GameObject>> ownedObjects_;

	Math::Vector3 desiredVelocity_{};
	std::string currentAnimationName_;

	static constexpr const char* kRootMotionBoneName = "mixamorig:Hips";
	float attackCooldownTimer_ = 0.0f;

	size_t patrolIndex_ = 0;
	bool hasTarget_ = false;
	TransformComponent* targetTransform_ = nullptr;

	bool isDead_ = false;
	float despawnTimer_ = 0.0f;

	ScopedSubscriber collisionSubscriber_;
	ScopedSubscriber parriedSubscriber_;
	ScopedSubscriber diedSubscriber_;

	std::unique_ptr<IBTNode<EnemyAIController>> root_;
};