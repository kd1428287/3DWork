#include "EnemyAIController.h"
#include "../../../Systems/TimeScaleEvents.h"

void EnemyAIController::UpdateTargetAcquisition()
{
	targetTransform_ = FindPlayerTransform();
	if (targetTransform_ == nullptr || transform_ == nullptr) {
		hasTarget_ = false;
		return;
	}

	const float distSq = (targetTransform_->GetPosition() - transform_->GetPosition()).LengthSquared();
	if (hasTarget_) {
		// 既に捕捉中: loseTargetRangeより離れたら見失う。
		if (distSq > data_.loseTargetRange * data_.loseTargetRange) {
			hasTarget_ = false;
		}
	}
	else {
		// 未捕捉: detectionRange以内に入ったら新規に捕捉する。
		if (distSq <= data_.detectionRange * data_.detectionRange) {
			hasTarget_ = true;
		}
	}
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

const EnemyAttackDefinition* EnemyAIController::ChooseAttack() const
{
	if (!HasTarget()) return nullptr;
	const float dist = DistanceToTarget();

	float totalWeight = 0.0f;
	for (const auto& atk : data_.attacks) {
		if (dist <= atk.maxRange) totalWeight += atk.weight;
	}
	if (totalWeight <= 0.0f) return nullptr;

	// 【要確認】std::rand()を使った簡易な重み付き抽選
	float roll = (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * totalWeight;
	for (const auto& atk : data_.attacks) {
		if (dist > atk.maxRange) continue;
		roll -= atk.weight;
		if (roll <= 0.0f) return &atk;
	}

	// 浮動小数の誤差でここまで抜けてきた場合のフォールバック。
	for (const auto& atk : data_.attacks) {
		if (dist <= atk.maxRange) return &atk;
	}
	return nullptr;
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

// --- 被弾処理 -----------------------------------------------------------
void EnemyAIController::OnCollisionEnter(const CollisionSystem::CollisionEnterEvent& e)
{
	if (isDead_) return;
	if (e.selfShapeName != "HurtBox") return;

	AttackSourceComponent* attack = e.otherObject->GetComponent<AttackSourceComponent>();
	if (attack == nullptr) return;

	// 多段ヒット防止
	if (attack->alreadyHit.count(GetOwner()) > 0) return;
	attack->alreadyHit.insert(GetOwner());

	RequestHitStopEvent(*GetOwner()->GetContext()->eventBus, 0.f, 0.1f);

	if (healthComponent_ != nullptr) {
		healthComponent_->TakeDamage(attack->damage);
	}

	if (behavior_) {
		behavior_->OnHit(this, *attack);
	}
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
	despawnTimer_ = behavior_ ? behavior_->GetDespawnDelay() : 1.5f;

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