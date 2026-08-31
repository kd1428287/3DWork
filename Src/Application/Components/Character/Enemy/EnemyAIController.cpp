#include "EnemyAIController.h"
#include <cmath>
#include <cstdlib>

void EnemyAIController::BuildTree()
{
	auto attackSeq = std::make_unique<BTSequence<EnemyAIController>>();
	attackSeq->AddChild(std::make_unique<BTCondition<EnemyAIController>>(
		[](EnemyAIController* c) { return c->IsTargetInAttackRange(); }));
	attackSeq->AddChild(std::make_unique<EnemyActionAttack>());

	auto chaseSeq = std::make_unique<BTSequence<EnemyAIController>>();
	chaseSeq->AddChild(std::make_unique<BTCondition<EnemyAIController>>(
		[](EnemyAIController* c) { return c->HasTarget(); }));
	chaseSeq->AddChild(std::make_unique<EnemyActionChase>());

	// 待機→巡回のループ。EnemyActionIdleが指定秒数Runningを返し続け、
	// Successで初めてEnemyActionPatrolへ進む(BTSequenceのresumption動作)。
	// Patrolが1地点に到達してSuccessを返すと、このSequence自体がSuccessで
	// 完了してrunningIndex_が0へ戻るため、次にSelectorから呼ばれた時は
	// 再びIdleから始まる(=次のウェイポイントでまた足を止める)。
	auto patrolSeq = std::make_unique<BTSequence<EnemyAIController>>();
	patrolSeq->AddChild(std::make_unique<EnemyActionIdle>());
	patrolSeq->AddChild(std::make_unique<EnemyActionPatrol>());

	// 優先度順(攻撃 > 追跡 > 待機/巡回)。reactive版BTSelectorのため、
	// 毎フレーム必ず攻撃条件から評価し直す(追跡中でも間合いに入った瞬間に
	// 即座に攻撃へ切り替わる)。
	auto selector = std::make_unique<BTSelector<EnemyAIController>>();
	selector->AddChild(std::move(attackSeq));
	selector->AddChild(std::move(chaseSeq));
	selector->AddChild(std::move(patrolSeq));

	root_ = std::move(selector);
}

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
	// PlayerLockOnComponentと同じ方式(ObjectManager経由の線形走査)。
	// 敵の数・呼び出し頻度(1体あたり毎フレーム1回)のポートフォリオ規模
	// では許容範囲という判断も同じ。
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

void EnemyAIController::FaceHorizontalTarget(const Math::Vector3& targetPosition)
{
	if (transform_ == nullptr) return;

	Math::Vector3 dir = targetPosition - transform_->GetPosition();
	dir.y = 0.0f;
	if (dir.LengthSquared() < 1e-6f) return; // 真上/真下等、水平差が無い場合は向きを変えない

	dir.Normalize();

	// 【実機で確認・修正済み】atan2(dir.x, dir.z)のままだと、狙った方向の
	// ちょうど180度反対を向く症状を確認したため、dirを反転させてから
	// yawを求めている(BruteAIController時代に確認済み)。
	const float yaw = std::atan2(-dir.x, -dir.z);
	transform_->SetRotation(Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yaw));
}

// --- 被弾/死亡処理 ---------------------------------------------------
// 旧EnemyStatusController::OnCollisionEnter/OnDiedの責務を復元したもの。
// BruteAIController時代はここが丸ごと欠落しており、ダメージが一切
// 入らなかった(唯一の実装へ昇格させる過程で発覚)。
//
// 【スコープ外のまま残っている点】
// ノックバック等の被弾リアクション(吹っ飛び演出)は実装していない。
// ダメージ・体幹削り・最終的な死亡は機能するが、殴られてもその場に
// 立ち続ける(=硬直や仰け反りが無い)点は未実装として残っている。
void EnemyAIController::OnCollisionEnter(const CollisionSystem::CollisionEnterEvent& e)
{
	if (isDead_) return;
	if (e.selfShapeName != "HurtBox") return;

	AttackSourceComponent* attack = e.otherObject->GetComponent<AttackSourceComponent>();
	if (attack == nullptr) return;

	// 多段ヒット防止。同じ攻撃(=HitBoxがenabled=trueになっている間)で
	// 既にこの相手(自分)へヒット済みなら無視する。
	if (attack->alreadyHit.count(GetOwner()) > 0) return;
	attack->alreadyHit.insert(GetOwner());

	if (healthComponent_ != nullptr) {
		healthComponent_->TakeDamage(attack->damage);
	}
	if (postureComponent_ != nullptr) {
		postureComponent_->AddPostureDamage(attack->postureDamage);
		if (postureComponent_->IsBroken()) {
			// TODO: 崩し状態への反応(専用の被弾リアクション)は
			// スコープ外のため未実装。
		}
	}
}

void EnemyAIController::OnDied()
{
	if (isDead_) return;
	isDead_ = true;
	despawnTimer_ = kDespawnDelay;

	// 移動・当たり判定を止める。当たり判定を無効化することで、以降
	// CollisionSystemがこのGameObjectをペア判定の対象から除外し、
	// OnCollisionEnterが呼ばれなくなる。
	StopMovement();
	if (movementComponent_ != nullptr) movementComponent_->SetEnabled(false);
	if (ColliderComponent* collider = GetOwner()->GetComponent<ColliderComponent>()) {
		collider->SetEnabled(false);
	}

	// TODO: 死亡アニメーション/エフェクトは未実装のためコメントアウト
	// (PlayerState.cpp::StateStagger::Enterと同じ考え方)。
	// PlayAnimation("Death", false, kDespawnDelay);
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
