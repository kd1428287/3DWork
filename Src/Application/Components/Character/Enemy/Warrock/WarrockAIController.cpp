#include "WarrockAIController.h"
#include <cmath>
#include <cstdlib>

void WarrockAIController::BuildTree()
{
	auto reactionSeq = std::make_unique<BTSequence<WarrockAIController>>();
	reactionSeq->AddChild(std::make_unique<BTCondition<WarrockAIController>>(
		[](WarrockAIController* c) { return c->IsHitReactionPending(); }));
	reactionSeq->AddChild(std::make_unique<WarrockActionHitReaction>());

	auto roarSeq = std::make_unique<BTSequence<WarrockAIController>>();
	roarSeq->AddChild(std::make_unique<BTCondition<WarrockAIController>>(
		[](WarrockAIController* c) { return c->IsRoarPending(); }));
	roarSeq->AddChild(std::make_unique<WarrockActionRoar>());

	auto attackSeq = std::make_unique<BTSequence<WarrockAIController>>();
	attackSeq->AddChild(std::make_unique<BTCondition<WarrockAIController>>(
		[](WarrockAIController* c) { return c->IsTargetInAttackRange(); }));
	attackSeq->AddChild(std::make_unique<WarrockActionAttack>());

	auto chaseSeq = std::make_unique<BTSequence<WarrockAIController>>();
	chaseSeq->AddChild(std::make_unique<BTCondition<WarrockAIController>>(
		[](WarrockAIController* c) { return c->HasTarget(); }));
	chaseSeq->AddChild(std::make_unique<WarrockActionChase>());

	// 優先度順(被弾リアクション > 咆哮 > 攻撃 > 追跡 > 待機)。
	// reactive版BTSelectorのため、毎フレーム必ず先頭(被弾リアクション)
	// から評価し直す。
	auto selector = std::make_unique<BTSelector<WarrockAIController>>();
	selector->AddChild(std::move(reactionSeq));
	selector->AddChild(std::move(roarSeq));
	selector->AddChild(std::move(attackSeq));
	selector->AddChild(std::move(chaseSeq));
	selector->AddChild(std::make_unique<WarrockActionIdle>());

	root_ = std::move(selector);
}

void WarrockAIController::UpdateTargetAcquisition()
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

TransformComponent* WarrockAIController::FindPlayerTransform() const
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

const EnemyAttackDefinition* WarrockAIController::ChooseAttack() const
{
	if (!HasTarget()) return nullptr;
	const float dist = DistanceToTarget();

	float totalWeight = 0.0f;
	for (const auto& atk : data_.attacks) {
		if (dist <= atk.maxRange) totalWeight += atk.weight;
	}
	if (totalWeight <= 0.0f) return nullptr;

	// 【要確認】std::rand()を使った簡易な重み付き抽選。プロジェクト側に
	// 専用の乱数ユーティリティがあるなら、そちらに差し替えて構わない。
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

void WarrockAIController::FaceHorizontalTarget(const Math::Vector3& targetPosition)
{
	if (transform_ == nullptr) return;

	Math::Vector3 dir = targetPosition - transform_->GetPosition();
	dir.y = 0.0f;
	if (dir.LengthSquared() < 1e-6f) return; // 真上/真下等、水平差が無い場合は向きを変えない

	dir.Normalize();

	const float yaw = std::atan2(-dir.x, -dir.z);
	transform_->SetRotation(Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yaw));
}