#pragma once
#include "Application/Definitions/Character/Enemy/EnemyAIData.h"
#include "Application/Definitions/Character/Enemy/EnemyBlackboardKeys.h"
#include "../../../Tags/IMovementSource.h"
#include "../../../Tags/IHitReactionQuery.h"

#include "Application/Definitions/Character/Enemy/IEnemyBehavior.h"
#include "Application/Definitions/Character/Common/BehaviorTree/IBTNode.h"
#include "Application/Definitions/Character/Common/BehaviorTree/Blackboard.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTWeightedAttackAction.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTNodeStatus.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTComposite.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTCondition.h"
#include "Application/Definitions/Character/Common/BehaviorTree/BTOneShotAnimationAction.h"
#include "../Common/AttackSourceComponent.h"
#include "../Common/PostureComponent.h"
#include "../Common/HealthComponent.h"
#include "../Common/WeaponSetComponent.h"
#include "../Common/HitReactionComponent.h"

#include "../../../Physics/Movement/MovementComponent.h"
#include "../../../Physics/Collision/ColliderComponent.h"
#include "../../../Graphics/Animation/FacingDirectionComponent.h"
#include "../../../Graphics/Animation/ModelAnimatorComponent.h"

#include "../Player/PlayerStatusController.h"
#include "Application/Core/EventBus/Events/HealthEvents.h"


// ============================================================
// 【WeaponSetComponent/HitReactionComponentへの統合について】
// 以前はweaponCollider_/weaponAttackSource_という生の2Handleを自前で
// 持ち、被弾処理(OnCollisionEnter)も自前で実装していたが、これらは
// Player/Enemy共通の設計として作られたWeaponSetComponent/
// HitReactionComponentへ置き換えた(PlayerStatusController::Awake()と
// 同じ配線)。
//
// HitReactionComponent::OnCollisionEnter()は多段ヒット防止・ダメージ
// 適用・ノックバック・体幹ダメージ・体幹崩壊判定まで、以前このクラスの
// OnCollisionEnter()が自前でやっていたことをほぼそのまま代替するため、
// 旧OnCollisionEnter()は削除しIHitReactionQueryの実装へ置き換えた。
// Enemy(少なくとも現状のWarrock)はガード/パリィといった防御行動を
// 持たないため、IsGuarding()/IsInParryWindow()は常にfalseを返し、
// HitReactionComponent側は常に「通常被弾」の分岐を通ってEnterStagger()
// を呼ぶ。
// ============================================================
class EnemyAIController : public ComponentBase, public IMovementSource, public IHitReactionQuery
{
public:
	EnemyAIController(GameObject* owner, const EnemyAIData& data, std::unique_ptr<IEnemyBehavior> behavior)
		: ComponentBase(owner), data_(data), behavior_(std::move(behavior)) {
	}

	void Awake() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		movementComponent_ = GetOwner()->GetComponent<MovementComponent>();
		modelAnimatorComponent_ = GetOwner()->GetComponent<ModelAnimatorComponent>();
		postureComponent_ = GetOwner()->GetComponent<PostureComponent>();
		healthComponent_ = GetOwner()->GetComponent<HealthComponent>();
		weaponSet_ = GetOwner()->GetComponent<WeaponSetComponent>();
		// 無くてもよい(任意)。存在する場合のみルートモーション中の
		// 向き自動追従の一時停止に使う(クラス冒頭コメント参照)。
		facingDirectionComponent_ = GetOwner()->GetComponent<FacingDirectionComponent>();

		if (movementComponent_ != nullptr) {
			movementComponent_->SetMovementSource(this);
		}

		// 自分自身をIHitReactionQueryとして登録(PlayerStatusController::
		// Awake()と同じ配線)。武器の鍔迫り合いエフェクト用にメイン武器も
		// 渡しておく。
		if (HitReactionComponent* hitReaction = GetOwner()->GetComponent<HitReactionComponent>()) {
			hitReaction->SetQuerySource(this);
			if (weaponSet_ != nullptr) {
				if (WeaponComponent* mainWeapon = weaponSet_->GetWeapon(kMainWeaponSlot)) {
					hitReaction->SetWeapon(Handle<WeaponComponent>(mainWeapon));
				}
			}
		}

		EventBus& localBus = GetOwner()->GetLocalEventBus();

		if (healthComponent_ != nullptr) {
			const SubscriptionId diedId = localBus.Subscribe<HealthDiedEvent>(
				[this](const HealthDiedEvent&) { OnDied(); });
			diedSubscriber_ = ScopedSubscriber(&localBus, diedId);
		}

		const SubscriptionId parriedId = localBus.Subscribe<AttackSourceComponent::ParriedEvent>(
			[this](const AttackSourceComponent::ParriedEvent& e) { OnParried(e); });
		parriedSubscriber_ = ScopedSubscriber(&localBus, parriedId);

		root_ = behavior_->BuildTree(this);
		behavior_->OnSpawned(this);
	}

	void Update(float deltaTime) override
	{
		if (isDead_) {
			despawnTimer_ -= deltaTime;
			if (despawnTimer_ <= 0.0f) {
				RequestDespawn();
			}
			return;
		}

		if (attackCooldownTimer_ > 0.0f) {
			attackCooldownTimer_ -= deltaTime;
			if (attackCooldownTimer_ < 0.0f) attackCooldownTimer_ = 0.0f;
		}
		// BTノード(将来のノードグラフエディタが組むConditionを含む)が
		// このフレームのクールダウン状態を参照できるよう、Tick()の前に
		// Blackboardへ書き込んでおく。
		blackboard_.SetBool(EnemyBlackboardKeys::IsAttackOnCooldown, attackCooldownTimer_ > 0.0f);

		UpdateTargetAcquisition();
		// attackSeqのConditionをBTCompareBoolCondition化できるよう、
		// IsTargetInAttackRange()の計算結果もBlackboardへ公開しておく
		// (EnemyBlackboardKeys.h「今後の追加候補」を実際に追加した)。
		// 計算自体は引き続きIsTargetInAttackRange()側のC++ロジックが行う。
		blackboard_.SetBool(EnemyBlackboardKeys::IsTargetInAttackRange, IsTargetInAttackRange());
		blackboard_.SetBool(EnemyBlackboardKeys::IsTargetInGapCloserRange, IsTargetInGapCloserRange());

		if (root_ != nullptr) root_->Tick(this, deltaTime);

		ApplyRootMotion();
	}

	// --- IMovementSourceの実装 ---------------------------------------------
	Math::Vector3 GetDesiredVelocity() override { return desiredVelocity_; }

	// --- IHitReactionQueryの実装 --------------------------------------------
	// Enemy(少なくとも現状のWarrock)はガード/パリィといった防御行動を
	// 持たないため、常にfalseを返す。HitReactionComponent::
	// OnCollisionEnter()はこれを見て常に「通常被弾」の分岐(else節)を
	// 通ってEnterStagger()を呼ぶ。防御行動を持つ敵種が今後増えたら、
	// この実装をEnemyAIData側のフラグ等で差し替えられるようにすること。
	bool IsGuarding() const override { return false; }
	bool IsInParryWindow() const override { return false; }
	void NotifyParrySuccess() override {}
	void NotifyGuardHit() override {}

	// 通常被弾の結果(体幹が壊れたか=isLarge)を敵種固有のBehaviorへ
	// そのまま橋渡しするだけ(クラス冒頭コメント参照)。durationは
	// 意図的に使わない(IEnemyBehavior::OnStaggered()コメント参照)。
	void EnterStagger(bool isLarge, float duration) override {
		if (behavior_) behavior_->OnStaggered(this, isLarge, duration);
	}

	// --- 実行層API(各Behaviorが組み立てるActionノードから呼ばれる) --------
	const EnemyAIData& GetData() const { return data_; }

	// BTノードから見える動的な実行時状態の置き場。EnemyAIData(敵種ごとの
	// 静的なチューニング値)とは対になる関係で、こちらは「今この瞬間の
	// 状況」を表す(EnemyBlackboardKeys.h参照)。値の計算自体は引き続き
	// このクラス(EnemyAIController)がC++で行い、Blackboardはその
	// 結果をBTノードへ公開するための置き場でしかない。
	Blackboard& GetBlackboard() { return blackboard_; }
	const Blackboard& GetBlackboard() const { return blackboard_; }

	Math::Vector3 GetPosition() const {
		return transform_ != nullptr ? transform_->GetPosition() : Math::Vector3::Zero;
	}

	// 索敵のヒステリシス込みの「今ターゲットを捕捉しているか」。
	// UpdateTargetAcquisition()が毎フレームBlackboardへ書き込む値を
	// 読むだけの薄いアクセサ(以前はhasTarget_という専用メンバを別途
	// 持っていたが、Blackboardと二重管理になるため廃止した)。
	bool HasTarget() const { return blackboard_.GetBoolOr(EnemyBlackboardKeys::HasTarget, false); }

	float DistanceToTarget() const {
		return blackboard_.GetFloatOr(EnemyBlackboardKeys::DistanceToTarget, FLT_MAX);
	}

	bool IsTargetInAttackRange() const {
		if (!HasTarget()) return false;
		const float dist = DistanceToTarget();
		for (const auto& atk : data_.attacks) {
			if (dist >= atk.minRange && dist <= atk.maxRange) return true;
		}
		return false;
	}

	// gapCloserAttacksのいずれかの間合い内にいるか。
	bool IsTargetInGapCloserRange() const {
		if (!HasTarget()) return false;
		const float dist = DistanceToTarget();
		for (const auto& atk : data_.gapCloserAttacks) {
			if (dist >= atk.minRange && dist <= atk.maxRange) return true;
		}
		return false;
	}

	const EnemyAttackDefinition* ChooseAttack() const;
	const EnemyAttackDefinition* ChooseGapCloserAttack() const;

	// EnemyAIData::attackIntervalDuration中かどうか。実体は
	// attackCooldownTimer_(内部実装詳細のタイマー)だが、BTノードから
	// 見える値としてはBlackboard(EnemyBlackboardKeys::IsAttackOnCooldown)
	// を唯一の実体とする(Update()が毎フレーム書き込む)。
	bool IsAttackOnCooldown() const { return blackboard_.GetBoolOr(EnemyBlackboardKeys::IsAttackOnCooldown, false); }
	void NotifyAttackCompleted() { attackCooldownTimer_ = data_.attackIntervalDuration; }

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
		if (HasTarget() && targetTransform_ != nullptr) return targetTransform_->GetPosition();
		return GetPosition();
	}

	// 【契約】MovementComponentはdesiredVelocity_(=ここで受け取るvelocity)
	// に正規化済みの方向のみが渡される前提で、自身のspeed_を掛けて
	// 最終的な速度にする(MovementComponent.h冒頭コメント参照)。
	// 大きさ(速度)はここではなくSetMovementSpeed()側で設定すること。
	void SetDesiredVelocity(const Math::Vector3& velocity) { desiredVelocity_ = velocity; }
	void StopMovement() { desiredVelocity_ = Math::Vector3::Zero; }

	// MovementComponentへ実際の移動速度を反映する薄い委譲
	// (PlayerCombatMovementComponent::ApplyMovementState()と同じ考え方)。
	// EnemyAIData::patrolSpeed/chaseSpeedのような「今回はどの速度で
	// 動きたいか」の判断はBT Action側(EnemyActionPatrol/Chase等)が行い、
	// これはそれをMovementComponentへ橋渡しするだけ。
	void SetMovementSpeed(float speed) {
		if (movementComponent_ != nullptr) movementComponent_->SetSpeed(speed);
	}

	void FaceHorizontalTarget(const Math::Vector3& targetPosition);

	// 同じループアニメーションを毎フレーム再生し直さないための薄いラッパー。
	void PlayAnimationIfChanged(const std::string& name, bool loop) {
		if (name == currentAnimationName_) return;
		PlayAnimation(name, loop);
	}

	void PlayAnimation(const std::string& name, bool loop = false, float targetDurationSeconds = -1.0f, bool useRootMotion = false) {
		currentAnimationName_ = name;
		if (modelAnimatorComponent_ != nullptr) {
			modelAnimatorComponent_->SetRootMotionEnabled(useRootMotion);
			modelAnimatorComponent_->Play(name, loop, targetDurationSeconds);
		}
		if (facingDirectionComponent_ != nullptr) {
			facingDirectionComponent_->SetUpdateEnabled(!useRootMotion);
		}
	}

	void PlayAnimation(const MotionClipData& clip) {
		if (modelAnimatorComponent_ == nullptr) return;
		modelAnimatorComponent_->Play(clip);
	}

	// --- 武器の攻撃判定 --------------------------------------------------
	// 【変更】以前はweaponCollider_/weaponAttackSource_という生の2Handleを
	// 自前で持っていたが、Player同様WeaponSetComponentへ委譲する形にした
	// (クラス冒頭コメント参照)。単一武器のEnemyを想定し、Playerと同じ
	// "Main"スロットへ登録する簡易版のセッターとして提供する
	// (複数武器を持つ敵種が出てきたら、スロット名を引数に取る形へ
	// 拡張すること)。
	void SetWeapon(Handle<WeaponComponent> weapon) {
		if (weaponSet_ != nullptr) weaponSet_->RegisterWeapon(kMainWeaponSlot, weapon);
	}

	// 【変更】以前は引数なし(常に唯一の武器を指す)だったが、Player同様
	// AttackData::weaponSlotsで対象スロットを指定できる形にした
	// (BTWeightedAttackAction::Tick()参照)。
	void SetWeaponHitBoxEnabled(const std::vector<std::string>& slots, bool enabled) {
		if (weaponSet_ != nullptr) weaponSet_->SetHitBoxEnabled(slots, enabled);
	}

	// 武器の軌跡エフェクト(Trailを持たない部位では何もしない。
	// WeaponComponent::SetTrailEmitting()参照)。
	void SetWeaponTrailEmitting(const std::vector<std::string>& slots, bool emitting) {
		if (weaponSet_ != nullptr) weaponSet_->SetTrailEmitting(slots, emitting);
	}

	// --- 体幹 --------------------------------------------------------------
	PostureComponent* GetPostureComponent() const { return postureComponent_; }

	// --- 死亡時の道連れ破棄 -------------------------------------------------
	void RegisterOwnedObject(Handle<GameObject> obj) {
		ownedObjects_.push_back(obj);
	}

private:
	void UpdateTargetAcquisition();
	TransformComponent* FindPlayerTransform() const;
	void OnParried(const AttackSourceComponent::ParriedEvent& e);
	void OnDied();
	void RequestDespawn();
	void ApplyRootMotion();

	EnemyAIData data_;
	std::unique_ptr<IEnemyBehavior> behavior_;

	// BTノードから見える動的な実行時状態(HasTarget/DistanceToTarget/
	// IsAttackOnCooldown等)の唯一の実体。GetBlackboard()参照。
	Blackboard blackboard_;

	TransformComponent* transform_ = nullptr;
	MovementComponent* movementComponent_ = nullptr;
	ModelAnimatorComponent* modelAnimatorComponent_ = nullptr;
	PostureComponent* postureComponent_ = nullptr;
	HealthComponent* healthComponent_ = nullptr;
	FacingDirectionComponent* facingDirectionComponent_ = nullptr;
	WeaponSetComponent* weaponSet_ = nullptr;

	std::vector<Handle<GameObject>> ownedObjects_;

	Math::Vector3 desiredVelocity_{};
	std::string currentAnimationName_;

	static constexpr const char* kRootMotionBoneName = "mixamorig:Hips";
	static constexpr const char* kMainWeaponSlot = "Main";
	float attackCooldownTimer_ = 0.0f;

	size_t patrolIndex_ = 0;
	// 直前に選ばれた技。同じ技の連続選択を避けるためChooseAttack()/
	// ChooseGapCloserAttack()が更新する(const関数内で更新するためmutable)。
	mutable const EnemyAttackDefinition* lastAttack_ = nullptr;
	// targetTransform_はBlackboardの対応する値型(bool/float/string)が無く、
	// BTノードから直接参照される想定でもない内部実装詳細のキャッシュの
	// ため、メンバとして持つ(HasTarget()はBlackboard経由。クラス冒頭の
	// EnemyBlackboardKeys関連コメント参照)。
	TransformComponent* targetTransform_ = nullptr;

	bool isDead_ = false;
	float despawnTimer_ = 0.0f;

	ScopedSubscriber parriedSubscriber_;
	ScopedSubscriber diedSubscriber_;

	std::unique_ptr<IBTNode<EnemyAIController>> root_;
};