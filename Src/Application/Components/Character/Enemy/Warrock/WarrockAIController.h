#pragma once
#include "WarrockAIData.h"
#include "../IEnemyAIController.h"
#include "../../BehaviorTree/IBTNode.h"
#include "../../BehaviorTree/BTNodeStatus.h"
#include "../../BehaviorTree/BTComposite.h"
#include "../../BehaviorTree/BTCondition.h"
#include "../../Data/HealthComponent.h"
#include "../../../../Systems/Collision/CollisionSystem.h"
#include "WarrockActions.h"
#include "../../Player/PlayerStatusController.h"
#include "../../../Transform/TransformComponent.h"
#include "../../../Movement/MovementComponent.h"
#include "../../../Movement/IMovementSource.h"
#include "../../../Animation/ModelAnimatorComponent.h"
#include "../../../Collision/ColliderComponent.h"
#include "../../../Collision/AttackSourceComponent.h"
#include "../../../../Core/Handle.h"

// ============================================================
// Warrock1体の「意思決定」と「実行」を1つのコンポーネントに統合した
// AIコントローラ。EnemyAIControllerとは完全に独立したWarrock専用の
// 実装(WarrockActions.hも同様)。EnemyAIData型を使っているのは
// EnemyDefinition::aiData(ファクトリー側が扱う共通データ構造)へ
// 載せるための「データの器」を合わせているだけで、ロジックの共有は
// 意図していない(WarrockAIData.h冒頭コメント参照)。
//
// 【IEnemyAIControllerについて】
// EnemyAIControllerと継承関係を持たないため、EnemyFactory側が敵種
// によらず武器を取り付けられるよう、最小限の共通インターフェース
// IEnemyAIControllerを実装する(IEnemyAIController.h参照)。
// RegisterOwnedObject()は死亡処理が未着手のためオーバーライドせず、
// 基底のデフォルト実装(何もしない)のままにしている。
//
// 【ツリー構成】(BuildTree()参照)
// Selector(reactive、優先度順)
// ├─ Sequence: Condition(被弾リアクション要求中?) → WarrockActionHitReaction
// ├─ Sequence: Condition(咆哮要求中?)             → WarrockActionRoar
// ├─ Sequence: Condition(攻撃間合い内?)           → WarrockActionAttack
// ├─ Sequence: Condition(ターゲットを検知中?)     → WarrockActionChase
// └─ WarrockActionIdle (待機。Warrockは持ち場を離れないため巡回は無い)
//
// 【割り込み行動(被弾リアクション/咆哮)の仕組み】
// 「要求フラグを立てる→最優先のCondition越しに拾われる→再生完了時に
// 自分でフラグを消費する」という共通パターンにしている
// (WarrockActions.h参照)。咆哮は登場時にStart()から自動的に1回
// 要求される他、(実装するかは未定の)第二フェーズ移行時にも同じ
// RequestRoar()を呼ぶだけで再利用できる想定。
//
// 【スコープ外】
// Dyingアニメーションを使った死亡処理は今回のBT骨格にはまだ含めていない
// (OnCollisionEnter()でダメージは適用しているが、HealthComponentが
//  0以下になった際の死亡分岐は未実装。将来HealthComponent側にDied
//  イベント等を追加した際にそこから死亡専用の処理を呼ぶ想定)。
// ============================================================
class WarrockAIController : public ComponentBase, public IMovementSource, public IEnemyAIController
{
public:
	explicit WarrockAIController(GameObject* owner, const EnemyAIData& data)
		: ComponentBase(owner), data_(data) {}

	void Start() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		movementComponent_ = GetOwner()->GetComponent<MovementComponent>();
		modelAnimatorComponent_ = GetOwner()->GetComponent<ModelAnimatorComponent>();
		healthComponent_ = GetOwner()->GetComponent<HealthComponent>();

		if (movementComponent_ != nullptr) {
			movementComponent_->SetMovementSource(this);
		}

		BuildTree();

		// HurtBoxへのCollisionEnterEventは、シーン共有バスではなくこの
		// GameObject自身のローカルバスにだけ届く(PlayerStatusController::
		// Start()の同種のコメント参照)。
		EventBus& localBus = GetOwner()->GetLocalEventBus();
		const SubscriptionId subscriptionId = localBus.Subscribe<CollisionSystem::CollisionEnterEvent>(
			[this](const CollisionSystem::CollisionEnterEvent& e) { OnCollisionEnter(e); });
		subscriber_ = ScopedSubscriber(&localBus, subscriptionId);

		// 登場時の咆哮を1回要求する(第二フェーズ移行時の再利用も想定。
		// WarrockActions.hクラスコメント参照)。
		RequestRoar();
	}

	void Update(float deltaTime) override
	{
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

	Math::Vector3 GetTargetPositionOrSelf() const {
		if (hasTarget_ && targetTransform_ != nullptr) return targetTransform_->GetPosition();
		return GetPosition();
	}

	void SetDesiredVelocity(const Math::Vector3& velocity) { desiredVelocity_ = velocity; }
	void StopMovement() { desiredVelocity_ = Math::Vector3::Zero; }

	// 水平方向だけを見てtargetPositionの方を向く(簡易実装、Slerp補間はしない)。
	//
	// 【要確認】+Z前方・DirectX左手系を想定したyaw角の算出をしている。
	// TransformComponentの回転表現/SetRotation()の実際のシグネチャに
	// 合わせて調整すること。
	void FaceHorizontalTarget(const Math::Vector3& targetPosition);

	// 同じループアニメーションを毎フレーム再生し直さないための薄いラッパー
	// (WarrockActionIdle/Chaseが継続して毎フレーム呼ぶ前提のため)。
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
	void SetWeapon(Handle<ColliderComponent> weaponCollider, Handle<AttackSourceComponent> weaponAttackSource) override {
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

	// --- 割り込み行動の要求API ---------------------------------------------
	// 外部(被弾イベント等)から呼び出し、次のBT評価でSmallReactionへ
	// 最優先で割り込ませる。要求はWarrockActionHitReactionが再生完了時に
	// 自分で消費する(BuildTree()参照)。
	void RequestHitReaction() { hitReactionPending_ = true; }
	bool IsHitReactionPending() const { return hitReactionPending_; }
	void ConsumeHitReactionRequest() { hitReactionPending_ = false; }

	// 咆哮の要求API。登場時にStart()から自動的に1回要求される他、
	// (実装するかは未定の)第二フェーズ移行時にも同じ経路で呼び出せる
	// ようにしている。
	void RequestRoar() { roarPending_ = true; }
	bool IsRoarPending() const { return roarPending_; }
	void ConsumeRoarRequest() { roarPending_ = false; }

private:
	// --- 被弾イベントの受信 --------------------------------------------------
	// HurtBoxへのCollisionEnterEvent受信時に呼ばれる。Player側のOnCollisionEnter
	// と同じ多段ヒット防止パターンを踏襲するが、Warrockにはガード/パリィが
	// 無いため分岐は行わず、ダメージ適用とSmallReactionの割り込み要求のみ行う。
	//
	// 【スコープ外】死亡処理(Dyingアニメーション)は引き続き未着手
	// (WarrockAIController.h冒頭コメント参照)。
	void OnCollisionEnter(const CollisionSystem::CollisionEnterEvent& e)
	{
		if (e.selfShapeName != "HurtBox") return;

		AttackSourceComponent* attack = e.otherObject->GetComponent<AttackSourceComponent>();
		if (attack == nullptr) return;

		// 多段ヒット防止(PlayerStatusController::OnCollisionEnterと同じ考え方)。
		if (attack->alreadyHit.count(GetOwner()) > 0) return;
		attack->alreadyHit.insert(GetOwner());

		if (healthComponent_ != nullptr) {
			healthComponent_->TakeDamage(attack->damage);
		}

		RequestHitReaction();
	}

	void BuildTree();
	void UpdateTargetAcquisition();
	TransformComponent* FindPlayerTransform() const;

	EnemyAIData data_;

	TransformComponent* transform_ = nullptr;
	MovementComponent* movementComponent_ = nullptr;
	ModelAnimatorComponent* modelAnimatorComponent_ = nullptr;

	HealthComponent* healthComponent_ = nullptr;
	ScopedSubscriber subscriber_;

	Handle<ColliderComponent> weaponCollider_;
	Handle<AttackSourceComponent> weaponAttackSource_;

	Math::Vector3 desiredVelocity_{};
	std::string currentAnimationName_;

	static constexpr const char* kRootMotionBoneName = "mixamorig:Hips";

	// ヒステリシス付き索敵状態。detectionRangeで捕捉し、loseTargetRangeより
	// 大きく離れるまでは捕捉状態を維持する(境界上でのChase/Idle往復を防ぐ)。
	bool hasTarget_ = false;
	TransformComponent* targetTransform_ = nullptr;

	// 割り込み行動の要求フラグ(RequestHitReaction()/RequestRoar()参照)。
	bool hitReactionPending_ = false;
	bool roarPending_ = false;

	std::unique_ptr<IBTNode<WarrockAIController>> root_;
};