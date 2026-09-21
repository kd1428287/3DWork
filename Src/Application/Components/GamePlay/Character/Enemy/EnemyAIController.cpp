#include "EnemyAIController.h"

void EnemyAIController::UpdateTargetAcquisition()
{
	targetTransform_ = FindPlayerTransform();
	if (targetTransform_ == nullptr || transform_ == nullptr) {
		blackboard_.SetBool(EnemyBlackboardKeys::HasTarget, false);
		blackboard_.SetFloat(EnemyBlackboardKeys::DistanceToTarget, FLT_MAX);
		return;
	}

	const float distSq = (targetTransform_->GetPosition() - transform_->GetPosition()).LengthSquared();

	// 現在Blackboardに乗っている値を読み、ヒステリシス込みで更新する
	// (以前はhasTarget_という専用メンバで同じことをしていた。
	// EnemyAIController.h冒頭コメント参照)。
	bool hasTarget = blackboard_.GetBoolOr(EnemyBlackboardKeys::HasTarget, false);
	if (hasTarget) {
		// 既に捕捉中: loseTargetRangeより離れたら見失う。
		if (distSq > data_.loseTargetRange * data_.loseTargetRange) {
			hasTarget = false;
		}
	}
	else {
		// 未捕捉: detectionRange以内に入ったら新規に捕捉する。
		if (distSq <= data_.detectionRange * data_.detectionRange) {
			hasTarget = true;
		}
	}

	blackboard_.SetBool(EnemyBlackboardKeys::HasTarget, hasTarget);
	blackboard_.SetFloat(EnemyBlackboardKeys::DistanceToTarget, std::sqrt(distSq));
}

TransformComponent* EnemyAIController::FindPlayerTransform() const
{
	SceneContext* context = GetOwner()->GetContext();
	if (context == nullptr || context->objectManager == nullptr) return nullptr;

	for (PlayerStatusController* player : context->objectManager->FindComponents<PlayerStatusController>()) {
		if (TransformComponent* t = player->GetOwner()->GetComponent<TransformComponent>()) {
			return t;
		}
	}
	return nullptr;
}

namespace
{
	// pool内からpredを満たす技を重み付き抽選する。
	template <typename Predicate>
	const EnemyAttackDefinition* WeightedPick(const std::vector<EnemyAttackDefinition>& pool, Predicate pred)
	{
		float totalWeight = 0.0f;
		for (const auto& atk : pool) if (pred(atk)) totalWeight += atk.weight;
		if (totalWeight <= 0.0f) return nullptr;

		// 【要確認】std::rand()を使った簡易な重み付き抽選
		float roll = (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * totalWeight;
		for (const auto& atk : pool) {
			if (!pred(atk)) continue;
			roll -= atk.weight;
			if (roll <= 0.0f) return &atk;
		}
		for (const auto& atk : pool) if (pred(atk)) return &atk;
		return nullptr;
	}

	// 直前の技(exclude)を除いて抽選。除外すると候補が無くなるなら除外しない。
	const EnemyAttackDefinition* ChooseFrom(const std::vector<EnemyAttackDefinition>& pool, float dist,
		const EnemyAttackDefinition* exclude)
	{
		auto inRange = [dist](const EnemyAttackDefinition& atk) {
			return dist >= atk.minRange && dist <= atk.maxRange;
			};

		if (const EnemyAttackDefinition* picked = WeightedPick(pool,
			[&](const EnemyAttackDefinition& atk) { return inRange(atk) && &atk != exclude; })) {
			return picked;
		}
		return WeightedPick(pool, inRange);
	}
}

const EnemyAttackDefinition* EnemyAIController::ChooseAttack() const
{
	if (!HasTarget()) return nullptr;
	lastAttack_ = ChooseFrom(data_.attacks, DistanceToTarget(), lastAttack_);
	return lastAttack_;
}

const EnemyAttackDefinition* EnemyAIController::ChooseGapCloserAttack() const
{
	if (!HasTarget()) return nullptr;
	lastAttack_ = ChooseFrom(data_.gapCloserAttacks, DistanceToTarget(), lastAttack_);
	return lastAttack_;
}

void EnemyAIController::FaceHorizontalTarget(const Math::Vector3& targetPosition)
{
	if (transform_ == nullptr) return;

	Math::Vector3 dir = targetPosition - transform_->GetPosition();
	dir.y = 0.0f;
	if (dir.LengthSquared() < 1e-6f) return;

	dir.Normalize();

	const float yaw = std::atan2(-dir.x, -dir.z);
	transform_->SetRotation(Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yaw));
}

// --- 被パリィ処理 ---------------------------------------------------------
void EnemyAIController::OnParried(const AttackSourceComponent::ParriedEvent& e)
{
	if (behavior_) {
		behavior_->OnParried(this, e);
	}
}

// --- 死亡処理 -------------------------------------------------------------
void EnemyAIController::OnDied()
{
	if (isDead_) return;
	isDead_ = true;
	despawnTimer_ = behavior_ ? behavior_->GetDespawnDelay(this) : 1.5f;

	StopMovement();
	if (movementComponent_ != nullptr) movementComponent_->SetEnabled(false);
	if (ColliderComponent* collider = GetOwner()->GetComponent<ColliderComponent>()) {
		collider->SetEnabled(false);
	}

	if (behavior_) {
		behavior_->OnDied(this);
	}
}

void EnemyAIController::RequestDespawn()
{
	SceneContext* context = GetOwner()->GetContext();
	if (context == nullptr || context->objectManager == nullptr) return;

	for (Handle<GameObject>& owned : ownedObjects_) {
		if (GameObject* obj = owned.Resolve()) {
			context->objectManager->Destroy(obj);
		}
	}

	context->objectManager->Destroy(GetOwner());
}

// --- ルートモーション -----------------------------------------------------
void EnemyAIController::ApplyRootMotion()
{
	if (modelAnimatorComponent_ == nullptr || transform_ == nullptr) return;

	Math::Vector3 localDelta = modelAnimatorComponent_->ConsumeRootMotionDelta();
	if (localDelta.LengthSquared() <= 0.0f) return;

	Math::Vector3 worldDelta = Math::Vector3::Transform(localDelta, transform_->GetRotation());
	transform_->Translate(worldDelta);
}