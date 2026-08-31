#pragma once
#include "BruteAIData.h"
#include "../../BehaviorTree/IBTNode.h"
#include "../../BehaviorTree/BTNodeStatus.h"
#include "../../BehaviorTree/BTComposite.h"
#include "../../BehaviorTree/BTCondition.h"
#include "BruteActions.h"
#include "../../Player/PlayerStatusController.h"
#include "../../../Transform/TransformComponent.h"
#include "../../../Movement/MovementComponent.h"
#include "../../../Movement/IMovementSource.h"
#include "../../../Animation/ModelAnimatorComponent.h"
#include "../../../Collision/ColliderComponent.h"
#include "../../../Collision/AttackSourceComponent.h"
#include "../../../../Core/Handle.h"

// ============================================================
// Brute1体の「意思決定」と「実行」を1つのコンポーネントに統合した
// AIコントローラ。
//
// 【設計判断: 意思決定と実行を分けない】
// 以前のEnemyStatusController/EnemyBTController構成では、実行層
// (EnemyStatusController、Brute/Boss継承)と決定層(EnemyBTController)を
// 別コンポーネントに分けていた。これは「GetComponent<T>()が具体型でしか
// 検索できないため、派生クラスをGetComponent<基底型>()で見つけられない」
// 問題を踏まえ、決定層からコンストラクタ経由で実行層を受け取る、という
// 迂回策が常に必要だった。
// 今回はBruteという1種の敵に対して継承ではなくデータ(BruteAIData)で
// 個体差を表現するため、この問題自体が発生しない。であれば2つの
// コンポーネントに分ける意味も薄いため、1つに統合してシンプルにした。
//
// 【データ駆動】
// パトロール地点・索敵距離・攻撃パターン(3種類程度、拡張可能)は全て
// BruteAIData(コンストラクタで受け取る)にまとまっている。個体ごとの
// 差はこのデータの中身を変えるだけで表現でき、C++の型を増やす必要はない。
//
// 【ツリー構成】(BuildTree()参照)
// Selector(reactive)
// ├─ Sequence: Condition(攻撃間合い内?) → BruteActionAttack
// ├─ Sequence: Condition(ターゲットを検知中?) → BruteActionChase
// └─ Sequence: BruteActionIdle → BruteActionPatrol (待機→巡回のループ)
//
// 【スコープ外】
// 被弾によるノックバック/死亡等の反応(以前のStateKnockback/StateDead相当)
// は今回のAI(何をすべきかの意思決定)とは別の関心事のため実装していない。
// 将来組み込む場合、ダメージイベント受信時にBT側のルートへ
// Reset()を呼んでから専用の割り込み処理へ切り替える、という形が
// 素直だと思われる。
// ============================================================
class BruteAIController : public ComponentBase, public IMovementSource
{
public:
	explicit BruteAIController(GameObject* owner, const BruteAIData& data)
		: ComponentBase(owner), data_(data) {}

	void Start() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		movementComponent_ = GetOwner()->GetComponent<MovementComponent>();
		modelAnimatorComponent_ = GetOwner()->GetComponent<ModelAnimatorComponent>();

		if (movementComponent_ != nullptr) {
			movementComponent_->SetMovementSource(this);
		}

		BuildTree();
	}

	void Update(float deltaTime) override
	{
		UpdateTargetAcquisition();
		if (root_ != nullptr) root_->Tick(this, deltaTime);
	}

	// --- IMovementSourceの実装 ---------------------------------------------
	Math::Vector3 GetDesiredVelocity() override { return desiredVelocity_; }

	// --- Actionノードから呼ばれるAPI ---------------------------------------
	const BruteAIData& GetData() const { return data_; }

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
	const BruteAttackDefinition* ChooseAttack() const;

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
	//
	// 【要確認】+Z前方・DirectX左手系を想定したyaw角の算出をしている。
	// TransformComponentの回転表現/SetRotation()の実際のシグネチャに
	// 合わせて調整すること。
	void FaceHorizontalTarget(const Math::Vector3& targetPosition);

	// 同じループアニメーションを毎フレーム再生し直さないための薄いラッパー
	// (BruteActionIdle/Patrol/Chaseが継続して毎フレーム呼ぶ前提のため)。
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
	// 生成元(BruteFactory相当)から、武器のColliderComponent/
	// AttackSourceComponentを登録してもらう想定
	// (EnemyStatusController::SetWeapon()と同じ考え方)。
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

private:
	void BuildTree();
	void UpdateTargetAcquisition();
	TransformComponent* FindPlayerTransform() const;

	BruteAIData data_;

	TransformComponent* transform_ = nullptr;
	MovementComponent* movementComponent_ = nullptr;
	ModelAnimatorComponent* modelAnimatorComponent_ = nullptr;

	Handle<ColliderComponent> weaponCollider_;
	Handle<AttackSourceComponent> weaponAttackSource_;

	Math::Vector3 desiredVelocity_{};
	std::string currentAnimationName_;

	size_t patrolIndex_ = 0;

	// ヒステリシス付き索敵状態。detectionRangeで捕捉し、loseTargetRangeより
	// 大きく離れるまでは捕捉状態を維持する(境界上でのChase/Patrol往復を防ぐ)。
	bool hasTarget_ = false;
	TransformComponent* targetTransform_ = nullptr;

	std::unique_ptr<IBTNode<BruteAIController>> root_;
};
