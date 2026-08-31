#include "BruteAIController.h"
#include <cmath>
#include <cstdlib>

void BruteAIController::BuildTree()
{
	auto attackSeq = std::make_unique<BTSequence<BruteAIController>>();
	attackSeq->AddChild(std::make_unique<BTCondition<BruteAIController>>(
		[](BruteAIController* c) { return c->IsTargetInAttackRange(); }));
	attackSeq->AddChild(std::make_unique<BruteActionAttack>());

	auto chaseSeq = std::make_unique<BTSequence<BruteAIController>>();
	chaseSeq->AddChild(std::make_unique<BTCondition<BruteAIController>>(
		[](BruteAIController* c) { return c->HasTarget(); }));
	chaseSeq->AddChild(std::make_unique<BruteActionChase>());

	// 待機→巡回のループ。BruteActionIdleが指定秒数Runningを返し続け、
	// Successで初めてBruteActionPatrolへ進む(BTSequenceのresumption動作)。
	// Patrolが1地点に到達してSuccessを返すと、このSequence自体がSuccessで
	// 完了してrunningIndex_が0へ戻るため、次にSelectorから呼ばれた時は
	// 再びIdleから始まる(=次のウェイポイントでまた足を止める)。
	auto patrolSeq = std::make_unique<BTSequence<BruteAIController>>();
	patrolSeq->AddChild(std::make_unique<BruteActionIdle>());
	patrolSeq->AddChild(std::make_unique<BruteActionPatrol>());

	// 優先度順(攻撃 > 追跡 > 待機/巡回)。reactive版BTSelectorのため、
	// 毎フレーム必ず攻撃条件から評価し直す(追跡中でも間合いに入った瞬間に
	// 即座に攻撃へ切り替わる)。
	auto selector = std::make_unique<BTSelector<BruteAIController>>();
	selector->AddChild(std::move(attackSeq));
	selector->AddChild(std::move(chaseSeq));
	selector->AddChild(std::move(patrolSeq));

	root_ = std::move(selector);
}

void BruteAIController::UpdateTargetAcquisition()
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

TransformComponent* BruteAIController::FindPlayerTransform() const
{
	// PlayerLockOnComponent/EnemyBTControllerと同じ方式(ObjectManager経由の
	// 線形走査)。敵の数・呼び出し頻度(1体あたり毎フレーム1回)の
	// ポートフォリオ規模では許容範囲という判断も同じ。
	SceneContext* context = GetOwner()->GetContext();
	if (context == nullptr || context->objectManager == nullptr) return nullptr;

	for (PlayerStatusController* player : context->objectManager->FindComponents<PlayerStatusController>()) {
		if (TransformComponent* t = player->GetOwner()->GetComponent<TransformComponent>()) {
			return t;
		}
	}
	return nullptr;
}

const BruteAttackDefinition* BruteAIController::ChooseAttack() const
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

void BruteAIController::FaceHorizontalTarget(const Math::Vector3& targetPosition)
{
	if (transform_ == nullptr) return;

	Math::Vector3 dir = targetPosition - transform_->GetPosition();
	dir.y = 0.0f;
	if (dir.LengthSquared() < 1e-6f) return; // 真上/真下等、水平差が無い場合は向きを変えない

	dir.Normalize();

	const float yaw = std::atan2(-dir.x, -dir.z);
	transform_->SetRotation(Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yaw));
}