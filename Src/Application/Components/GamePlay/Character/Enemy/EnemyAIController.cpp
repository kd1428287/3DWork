#include "EnemyAIController.h"
#include "../Common/CharacterEvents.h"
#include "Application/Core/EventBus/Events/AudioEvents.h"

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
	const SceneContext* context = GetOwner()->GetContext();
	if (context == nullptr) return nullptr;

	// PlayerStatusController(Playerの内部実装)を直接探していたのをやめ、
	// SceneContext::player(PlayerComponentが自己登録する弱参照)経由にした。
	// activeCamera/lockedTargetと同じ「シーンに1つだけの既知の対象」の
	// 扱いに揃え、EnemyAIControllerはPlayer側の具体クラスを一切知らずに済む。
	GameObject* player = context->player.Resolve();
	return (player != nullptr) ? player->GetComponent<TransformComponent>() : nullptr;
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

// --- 効果音 ---------------------------------------------------------------
void EnemyAIController::PlaySE(const std::string& id) const
{
	if (id.empty()) return;
	Events::PublishSoundPlayEvent(*GlobalEventBus::Instance(), id);
}

// --- 予備動作エフェクト ---------------------------------------------------
Math::Vector3 EnemyAIController::ComputeTelegraphPosition() const
{
	// 基準位置: anchorSlotの武器オブジェクト位置。見つからなければルート
	Math::Vector3 basePos = transform_->GetPosition();
	if (!telegraphData_.anchorSlot.empty() && weaponSet_ != nullptr) {
		if (WeaponComponent* weapon = weaponSet_->GetWeapon(telegraphData_.anchorSlot)) {
			if (TransformComponent* weaponTf = weapon->GetTransform()) {
				basePos = weaponTf->GetPosition();
			}
		}
	}
	// offsetは敵の向きに合わせて回す
	return basePos + Math::Vector3::Transform(telegraphData_.offset, transform_->GetRotation());
}

void EnemyAIController::PlayTelegraphEffect(const EnemyAttackTelegraphData& data)
{
	if (data.effectName.empty() || transform_ == nullptr) return;

	StopTelegraphEffect(); // 二重再生防止
	telegraphData_ = data;
	telegraphKey_ = "EnemyTelegraph_" + std::to_string(reinterpret_cast<uintptr_t>(this));
	Events::PublishEffectAttach(GetOwner()->GetSceneEventBus(), telegraphKey_, data.effectName, ComputeTelegraphPosition());
}

void EnemyAIController::UpdateTelegraphEffect()
{
	if (telegraphKey_.empty() || transform_ == nullptr) return;
	Events::PublishEffectPositionUpdate(GetOwner()->GetSceneEventBus(), telegraphKey_, ComputeTelegraphPosition());
}

void EnemyAIController::StopTelegraphEffect()
{
	if (telegraphKey_.empty()) return;
	Events::PublishEffectDetach(GetOwner()->GetSceneEventBus(), telegraphKey_);
	telegraphKey_.clear();
}

// --- 被パリィ処理 ---------------------------------------------------------
void EnemyAIController::OnParrySucceeded(const Events::ParrySucceeded& e)
{
	if (behavior_) {
		behavior_->OnParrySucceeded(this, e);
	}
}

// --- 死亡処理 -------------------------------------------------------------
void EnemyAIController::OnDied()
{
	if (isDead_) return;
	isDead_ = true;
	StopTelegraphEffect();
	despawnTimer_ = behavior_ ? behavior_->GetDespawnDelay(this) : 1.5f;

	PlaySE(sound_.death);

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
	StopTelegraphEffect();
	const SceneContext* context = GetOwner()->GetContext();
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