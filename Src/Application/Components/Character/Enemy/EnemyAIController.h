#pragma once
#include <cfloat>
#include <memory>
#include <string>
#include "EnemyAIData.h"
#include "EnemyActions.h"
#include "../BehaviorTree/IBTNode.h"
#include "../BehaviorTree/BTNodeStatus.h"
#include "../BehaviorTree/BTComposite.h"
#include "../BehaviorTree/BTCondition.h"
#include "../Player/PlayerStatusController.h"
#include "../../Transform/TransformComponent.h"
#include "../../Movement/MovementComponent.h"
#include "../../Movement/IMovementSource.h"
#include "../../Animation/ModelAnimatorComponent.h"
#include "../../Collision/ColliderComponent.h"
#include "../../Collision/AttackSourceComponent.h"
#include "../../../Systems/Collision/CollisionSystem.h"
#include "../Data/PostureComponent.h"
#include "../Data/HealthComponent.h"
#include "../../../Core/Handle.h"

// ============================================================
// 全ての敵種で共用する、唯一のAI実行/意思決定コンポーネント。
//
// 【この一般化について・経緯】
// 以前はEnemyStatusController(継承ベース、BruteStatusController/
// BossStatusControllerで敵種を出し分け)+EnemyBTController(意思決定)
// という2コンポーネント構成だった。継承ベースの実行層を全面的に廃止し、
// BT駆動のAIへ一本化するにあたり、Brute専用のつもりで作った
// BruteAIControllerが実際にはBrute固有の要素を持っていなかったため、
// そのままEnemyAIControllerへ改名して全敵種の唯一の実装とした。
// 敵種ごとの違いはEnemyAIData(コンストラクタで受け取る)の中身だけで
// 表現し、C++の型を増やす必要はない(EnemyAIData::CreateDebugBruteAIData()/
// CreateDebugBossAIData()参照)。
//
// 意思決定(BT)と実行(移動・アニメーション・武器制御)を1つの
// コンポーネントに統合しているのは、継承由来のGetComponent<T>()問題が
// 無くなったため、分ける動機(型解決の迂回)自体が消えたことによる。
//
// 【今回追加した被弾/死亡処理について】
// BruteAIController時代はここが丸ごと抜けており、敵を殴ってもダメージが
// 入らず、HPが0になっても消滅しないという欠落があった(唯一の実装として
// 昇格させるにあたって発覚)。旧EnemyStatusController::OnCollisionEnter/
// OnDied/RegisterOwnedObject/RequestDespawnの責務をここに復元している。
// ただしノックバック等の被弾リアクション(吹っ飛び演出)は今回もスコープ外
// のまま(意思決定AIとは別の関心事という以前の判断を踏襲)。ダメージ・
// 体幹削り・死亡は機能するが、殴られても見た目上その場に立ち続ける点は
// 未実装として残っている。
//
// 【ツリー構成】(BuildTree()参照)
// Selector(reactive)
// ├─ Sequence: Condition(攻撃間合い内?) → EnemyActionAttack
// ├─ Sequence: Condition(ターゲットを検知中?) → EnemyActionChase
// └─ Sequence: EnemyActionIdle → EnemyActionPatrol (待機→巡回のループ)
// ============================================================
class EnemyAIController : public ComponentBase, public IMovementSource
{
public:
	explicit EnemyAIController(GameObject* owner, const EnemyAIData& data)
		: ComponentBase(owner), data_(data) {}

	void Start() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		movementComponent_ = GetOwner()->GetComponent<MovementComponent>();
		modelAnimatorComponent_ = GetOwner()->GetComponent<ModelAnimatorComponent>();
		postureComponent_ = GetOwner()->GetComponent<PostureComponent>();
		healthComponent_ = GetOwner()->GetComponent<HealthComponent>();

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

		BuildTree();
	}

	void Update(float deltaTime) override
	{
		// 死亡後はBTを一切ティックしない。移動/当たり判定は死亡確定時に
		// 既に止めてある(OnDied()参照)ため、ここでは消滅までの猶予
		// タイマーだけを進める。
		if (isDead_) {
			despawnTimer_ -= deltaTime;
			if (despawnTimer_ <= 0.0f) {
				RequestDespawn();
			}
			return;
		}

		UpdateTargetAcquisition();
		if (root_ != nullptr) root_->Tick(this, deltaTime);
	}

	// --- IMovementSourceの実装 ---------------------------------------------
	Math::Vector3 GetDesiredVelocity() override { return desiredVelocity_; }

	// --- Actionノードから呼ばれるAPI ---------------------------------------
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

	// 現在の距離で使える攻撃パターンの中から重み付き抽選で1つ選ぶ。
	// 該当が無ければnullptr(実装は.cpp側)。
	const EnemyAttackDefinition* ChooseAttack() const;

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

	// 水平方向だけを見てtargetPositionの方を向く(簡易実装、Slerp補間はしない。
	// PlayerStatusController::FaceAttackTarget()と同じ考え方。実装は.cpp側)。
	void FaceHorizontalTarget(const Math::Vector3& targetPosition);

	// 同じループアニメーションを毎フレーム再生し直さないための薄いラッパー
	// (EnemyActionIdle/Patrol/Chaseが継続して毎フレーム呼ぶ前提のため)。
	void PlayAnimationIfChanged(const std::string& name, bool loop) {
		if (name == currentAnimationName_) return;
		PlayAnimation(name, loop);
	}

	void PlayAnimation(const std::string& name, bool loop = false, float targetDurationSeconds = -1.0f) {
		currentAnimationName_ = name;
		if (modelAnimatorComponent_ != nullptr) {
			modelAnimatorComponent_->Play(name, loop, targetDurationSeconds);
		}
	}

	// --- 武器の攻撃判定 --------------------------------------------------
	// 生成元(EnemyFactory)から、武器のColliderComponent/
	// AttackSourceComponentを登録してもらう想定。
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

	// --- 死亡時の道連れ破棄 -------------------------------------------------
	// このEnemyが生成した(=このEnemyが消えたら道連れで消えるべき)
	// 付随オブジェクト(武器・武器ソケット等)を登録する。EnemyFactory側で
	// 生成直後に呼んでもらう想定(以前のEnemyStatusController::
	// RegisterOwnedObject()と同じ役割)。
	void RegisterOwnedObject(Handle<GameObject> obj) {
		ownedObjects_.push_back(obj);
	}

private:
	void BuildTree();
	void UpdateTargetAcquisition();
	TransformComponent* FindPlayerTransform() const;
	void OnCollisionEnter(const CollisionSystem::CollisionEnterEvent& e);
	void OnDied();
	void RequestDespawn();

	EnemyAIData data_;

	TransformComponent* transform_ = nullptr;
	MovementComponent* movementComponent_ = nullptr;
	ModelAnimatorComponent* modelAnimatorComponent_ = nullptr;
	PostureComponent* postureComponent_ = nullptr;
	HealthComponent* healthComponent_ = nullptr;

	Handle<ColliderComponent> weaponCollider_;
	Handle<AttackSourceComponent> weaponAttackSource_;
	std::vector<Handle<GameObject>> ownedObjects_;

	Math::Vector3 desiredVelocity_{};
	std::string currentAnimationName_;

	size_t patrolIndex_ = 0;

	// ヒステリシス付き索敵状態。detectionRangeで捕捉し、loseTargetRangeより
	// 大きく離れるまでは捕捉状態を維持する(境界上でのChase/Patrol往復を防ぐ)。
	bool hasTarget_ = false;
	TransformComponent* targetTransform_ = nullptr;

	// 死亡演出から消滅までの猶予秒数(仮の値。旧StateDead::kDespawnDelayと同じ)。
	static constexpr float kDespawnDelay = 1.5f;
	bool isDead_ = false;
	float despawnTimer_ = 0.0f;

	ScopedSubscriber collisionSubscriber_;
	ScopedSubscriber diedSubscriber_;

	std::unique_ptr<IBTNode<EnemyAIController>> root_;
};
