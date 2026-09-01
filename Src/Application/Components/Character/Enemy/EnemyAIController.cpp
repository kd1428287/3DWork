#include "EnemyAIController.h"
#include <cmath>
#include <cstdlib>

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

// --- 被弾処理 -----------------------------------------------------------
// ダメージ適用と多段ヒット防止だけをここで行い、その先の反応(体幹削り・
// 被弾リアクション要求等)は敵種ごとに異なるためBehaviorへ委譲する
// (IEnemyBehavior::OnHit()参照)。
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

	if (behavior_) {
		behavior_->OnHit(this, *attack);
	}
}

// --- 死亡処理 -------------------------------------------------------------
// 移動停止・当たり判定無効化・消滅タイマー開始という全敵種共通の処理を
// ここで行い、Dyingアニメーション等の演出はBehavior::OnDied()へ委譲する。
// 消滅までの猶予秒数もBehavior::GetDespawnDelay()から取る(ボスは
// 死亡演出に合わせて長めの値を返す想定。WarrockBehavior::
// GetDespawnDelay()参照)。
void EnemyAIController::OnDied()
{
	if (isDead_) return;
	isDead_ = true;
	despawnTimer_ = behavior_ ? behavior_->GetDespawnDelay() : 1.5f;

	// 移動・当たり判定を止める。当たり判定を無効化することで、以降
	// CollisionSystemがこのGameObjectをペア判定の対象から除外し、
	// OnCollisionEnterが呼ばれなくなる。
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
// PlayerStatusController::ApplyRootMotion()と同じ考え方。ボーンのローカル
// (≒モデル)空間の移動量を、キャラクターの現在のワールド回転で変換して
// からワールド移動量として加算する(Vector3::Transform(vector, quaternion)
// は回転のみを適用し、平行移動は含まないため、そのままデルタの変換に使える)。
void EnemyAIController::ApplyRootMotion()
{
	if (modelAnimatorComponent_ == nullptr || transform_ == nullptr) return;

	Math::Vector3 localDelta = modelAnimatorComponent_->ConsumeRootMotionDelta();
	if (localDelta.LengthSquared() <= 0.0f) return;

	Math::Vector3 worldDelta = Math::Vector3::Transform(localDelta, transform_->GetRotation());
	transform_->Translate(worldDelta);
}