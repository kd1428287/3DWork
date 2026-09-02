#pragma once
#include <cfloat>
#include <memory>
#include <string>
#include "EnemyAIData.h"
#include "IEnemyBehavior.h"
#include "../BehaviorTree/BTWeightedAttackAction.h"
#include "../BehaviorTree/IBTNode.h"
#include "../BehaviorTree/BTNodeStatus.h"
#include "../BehaviorTree/BTComposite.h"
#include "../BehaviorTree/BTCondition.h"
#include "../BehaviorTree/BTOneShotAnimationAction.h"
#include "../Player/PlayerStatusController.h"
#include "../../Transform/TransformComponent.h"
#include "../../Movement/MovementComponent.h"
#include "../../Movement/IMovementSource.h"
#include "../../Movement/FacingDirectionComponent.h"
#include "../../Animation/ModelAnimatorComponent.h"
#include "../../Collision/ColliderComponent.h"
#include "../../Collision/AttackSourceComponent.h"
#include "../../../Systems/Collision/CollisionSystem.h"
#include "../Data/PostureComponent.h"
#include "../Data/HealthComponent.h"
#include "../../../Core/Handle.h"

// ============================================================
// 全ての敵種で共用する、唯一のAI実行コンポーネント。
//
// 【今回の再設計について】
// 以前はEnemyAIController/WarrockAIControllerという別々のコンポーネント
// を敵種ごとに用意し、IEnemyAIControllerという最小限の共通インター
// フェース経由でEnemyFactoryから武器を取り付けていた。しかし実行層
// (移動・索敵・アニメーション・武器制御)がほぼ完全に重複していたため、
// コンポーネント自体をこのEnemyAIController1種類に統合し、敵種ごとの
// 判断層・固有反応だけをIEnemyBehavior(コンストラクタで受け取る、
// IEnemyBehavior.h参照)へ切り出した。
//
// 現在デバッグ表示で動かしながら詳細を詰めているのはWarrock
// (WarrockBehavior)。被弾リアクション・咆哮・死亡演出といった
// Warrock側の要件を基準にOnHit()/OnDied()/OnSpawned()等のフックの
// 形を決めている。汎用敵(BruteBehavior)側はこの形に合わせて今後
// 育てていく想定で、現時点のBruteBehaviorの実装内容に引きずられて
// このコンポーネント自体の形を決めているわけではない。
//
// 【体幹(Posture)について】
// 体幹削り・パリィといった反応はWarrockには存在しない
// (WarrockBehavior::OnHit()参照)ため、共通処理には含めず、
// PostureComponentへのアクセスだけをGetPostureComponent()経由で
// Behavior側に提供している。体幹を使う敵種のBehaviorがOnHit()内で
// 自分から呼び出す形にした。
//
// 【ルートモーションについて】
// PlayerStatusControllerと同じ仕組み(ModelAnimatorComponent::
// SetRootMotionBoneName()/ConsumeRootMotionDelta())をそのまま使う。
// PlayAnimation()にuseRootMotion=trueを渡した技は、EnemyAttackDefinition::
// useRootMotionが立っている技としてBTWeightedAttackAction<T>から自動的に
// 渡される(EnemyAIData.h参照)。実際の移動適用はApplyRootMotion()
// (Update()から毎フレーム呼ぶ)側の役目。
//
// 【ルートモーション中の向き自動追従について・修正】
// FacingDirectionComponentは「前フレームからの位置差分」を移動方向とみなし
// 向きを追従させるが、ApplyRootMotion()による移動もこれをトリガーしてしまう。
// さらにApplyRootMotion()自体が「現在の向き」を使ってローカル→ワールド変換
// するため、向きが変わる→次フレームの変換方向が変わる→位置差分の方向が
// 変わる→また向きが変わる、という正のフィードバックループになり、
// ルートモーション再生中にモデルの向きが意図せず暴れる不具合があった
// (FacingDirectionComponent.h側にも同種の干渉についての教訓コメントあり)。
// Player側がuseRootMotion技でStepMove/向き自動更新を無効化しているのと
// 同じ考え方で、PlayAnimation()にuseRootMotion=trueが渡された間は
// FacingDirectionComponent::SetUpdateEnabled(false)で自動追従を止める。
//
// 【ツリー構成】敵種ごとに異なる(各Behavior::BuildTree()参照)。
// このコンポーネント自体はどの木が組まれるか関知しない。
// ============================================================
class EnemyAIController : public ComponentBase, public IMovementSource
{
public:
	EnemyAIController(GameObject* owner, const EnemyAIData& data, std::unique_ptr<IEnemyBehavior> behavior)
		: ComponentBase(owner), data_(data), behavior_(std::move(behavior)) {}

	void Start() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		movementComponent_ = GetOwner()->GetComponent<MovementComponent>();
		modelAnimatorComponent_ = GetOwner()->GetComponent<ModelAnimatorComponent>();
		postureComponent_ = GetOwner()->GetComponent<PostureComponent>();
		healthComponent_ = GetOwner()->GetComponent<HealthComponent>();
		// 無くてもよい(任意)。存在する場合のみルートモーション中の
		// 向き自動追従の一時停止に使う(クラス冒頭コメント参照)。
		facingDirectionComponent_ = GetOwner()->GetComponent<FacingDirectionComponent>();

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

		// 自分自身の攻撃がパリィされた時、AttackSourceComponent::ownerCharacter
		// (=自分自身)のローカルEventBusへParriedEventが発行される想定
		// (AttackSourceComponent::ParriedEvent冒頭コメント参照)。
		// 反応するかどうか・どう反応するかはBehavior側に委ねるため、
		// ここでは単に受け取ってOnParried()経由でbehavior_へ転送するだけ。
		const SubscriptionId parriedId = localBus.Subscribe<AttackSourceComponent::ParriedEvent>(
			[this](const AttackSourceComponent::ParriedEvent& e) { OnParried(e); });
		parriedSubscriber_ = ScopedSubscriber(&localBus, parriedId);

		root_ = behavior_->BuildTree(this);
		behavior_->OnSpawned(this);
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

		// 攻撃インターバルはBTのTick有無に関わらず常に進める(追跡中や
		// 待機中でも、次に攻撃可能になるまでの時間経過自体は止めない)。
		if (attackCooldownTimer_ > 0.0f) {
			attackCooldownTimer_ -= deltaTime;
			if (attackCooldownTimer_ < 0.0f) attackCooldownTimer_ = 0.0f;
		}

		UpdateTargetAcquisition();
		if (root_ != nullptr) root_->Tick(this, deltaTime);

		// ModelAnimatorComponentが今フレーム抽出したルートモーション移動量を
		// キャラクターのTransformへ反映する。root_->Tick()の後に置いているのは、
		// この中でPlayAnimation()による技の切り替えが起こりうるため、その
		// 切り替え後の状態も踏まえてから最後に一度だけ消費したいという理由
		// (PlayerStatusController::Update()と同じ考え方)。
		ApplyRootMotion();
	}

	// --- IMovementSourceの実装 ---------------------------------------------
	Math::Vector3 GetDesiredVelocity() override { return desiredVelocity_; }

	// --- 実行層API(各Behaviorが組み立てるActionノードから呼ばれる) --------
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
	// 該当が無ければnullptr(実装は.cpp側)。BTWeightedAttackAction<T>から
	// 呼ばれる。
	const EnemyAttackDefinition* ChooseAttack() const;

	// 攻撃のインターバル中かどうか。BuildTree()側の攻撃Sequenceの入り口
	// (Condition)でIsTargetInAttackRange()と併せてチェックする想定
	// (WarrockBehavior::BuildTree()参照)。インターバル中でも移動
	// (追跡/巡回/待機)は制限しない。
	bool IsAttackOnCooldown() const { return attackCooldownTimer_ > 0.0f; }

	// 攻撃1回(Windup+Active+Recovery)が完了した直後にBTWeightedAttackAction<T>
	// から呼ばれる(BTWeightedAttackAction.h参照)。EnemyAIData::
	// attackIntervalDurationをそのままクールダウンの残り時間として設定する。
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
		if (hasTarget_ && targetTransform_ != nullptr) return targetTransform_->GetPosition();
		return GetPosition();
	}

	void SetDesiredVelocity(const Math::Vector3& velocity) { desiredVelocity_ = velocity; }
	void StopMovement() { desiredVelocity_ = Math::Vector3::Zero; }

	// 水平方向だけを見てtargetPositionの方を向く(簡易実装、Slerp補間はしない。
	// PlayerStatusController::FaceAttackTarget()と同じ考え方。実装は.cpp側)。
	void FaceHorizontalTarget(const Math::Vector3& targetPosition);

	// 同じループアニメーションを毎フレーム再生し直さないための薄いラッパー。
	void PlayAnimationIfChanged(const std::string& name, bool loop) {
		if (name == currentAnimationName_) return;
		PlayAnimation(name, loop);
	}

	// useRootMotionは、この技がアニメーションクリップのルートモーションで
	// 動くかどうか(PlayerStatusController::PlayAnimationと同じ考え方)。
	// 呼ぶたびに必ずModelAnimatorComponent::SetRootMotionBoneName()で
	// 明示的に設定し直すことで、前回別の技がルートモーションを有効に
	// していた場合でも、この技がfalseなら確実に無効化される。
	// 実際にワールド移動量へ変換して適用するのはApplyRootMotion()
	// (Update()から毎フレーム呼ぶ)側の役目。
	//
	// 【修正】useRootMotionがtrueの間はFacingDirectionComponentの向き
	// 自動追従を止める。ApplyRootMotion()による位置移動を「入力移動」と
	// 誤認して向きを変え続け、その回転が次フレームのルートモーション
	// 変換方向にまで影響してフィードバックループになる不具合があった
	// (クラス冒頭コメント参照)。useRootMotionがfalseに戻った時は
	// 自動追従を再開する。
	void PlayAnimation(const std::string& name, bool loop = false, float targetDurationSeconds = -1.0f, bool useRootMotion = false) {
		currentAnimationName_ = name;
		if (modelAnimatorComponent_ != nullptr) {
			modelAnimatorComponent_->SetRootMotionBoneName(useRootMotion ? kRootMotionBoneName : "");
			modelAnimatorComponent_->Play(name, loop, targetDurationSeconds);
		}
		if (facingDirectionComponent_ != nullptr) {
			facingDirectionComponent_->SetUpdateEnabled(!useRootMotion);
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

	// --- 体幹 --------------------------------------------------------------
	// 体幹削り/パリィを使う敵種のBehaviorだけが、自分のOnHit()内から
	// 使う(IEnemyBehavior.h参照)。使わない敵種(Warrock等)は単に
	// 呼ばない。
	PostureComponent* GetPostureComponent() const { return postureComponent_; }

	// --- 死亡時の道連れ破棄 -------------------------------------------------
	// このEnemyが生成した(=このEnemyが消えたら道連れで消えるべき)
	// 付随オブジェクト(武器・武器ソケット等)を登録する。EnemyFactory側で
	// 生成直後に呼んでもらう想定。
	void RegisterOwnedObject(Handle<GameObject> obj) {
		ownedObjects_.push_back(obj);
	}

private:
	void UpdateTargetAcquisition();
	TransformComponent* FindPlayerTransform() const;
	void OnCollisionEnter(const CollisionSystem::CollisionEnterEvent& e);
	void OnParried(const AttackSourceComponent::ParriedEvent& e);
	void OnDied();
	void RequestDespawn();

	// ModelAnimatorComponent側で抽出されたルートモーション移動量
	// (ボーンのローカル/モデル空間、まだ回転を反映していない値)を、
	// キャラクターの現在の向きでワールド空間に変換してTransformへ加算する。
	// 実装は.cpp側(PlayerStatusController::ApplyRootMotion()と同じ考え方)。
	//
	// 【呼び出し順序の前提・未確認】ModelAnimatorComponent::Update()が
	// このフレーム内で既に実行済みである必要がある。GameObject側の
	// コンポーネント更新順が型をまたいでもAddComponentした順序通りに
	// 保証されるか未確認のため、EnemyFactory側でModelAnimatorComponentを
	// このコンポーネントより先にAddComponentするよう明示的に順序を揃えて
	// いる(EnemyFactory.cpp::BuildEnemy()参照)。
	void ApplyRootMotion();

	EnemyAIData data_;
	std::unique_ptr<IEnemyBehavior> behavior_;

	TransformComponent* transform_ = nullptr;
	MovementComponent* movementComponent_ = nullptr;
	ModelAnimatorComponent* modelAnimatorComponent_ = nullptr;
	PostureComponent* postureComponent_ = nullptr;
	HealthComponent* healthComponent_ = nullptr;
	// 無くてもよい(任意)。ルートモーション中の向き自動追従の一時停止に使う
	// (PlayAnimation()参照)。
	FacingDirectionComponent* facingDirectionComponent_ = nullptr;

	Handle<ColliderComponent> weaponCollider_;
	Handle<AttackSourceComponent> weaponAttackSource_;
	std::vector<Handle<GameObject>> ownedObjects_;

	Math::Vector3 desiredVelocity_{};
	std::string currentAnimationName_;

	// PlayAnimation(useRootMotion=true)の際にModelAnimatorComponent::
	// SetRootMotionBoneName()へ渡すボーン名。PlayerStatusController::
	// kRootMotionBoneNameと同じ値(Mixamoリグのhipボーン名)を想定している。
	static constexpr const char* kRootMotionBoneName = "mixamorig:Hips";

	// 攻撃1回終了後、次の攻撃を許可するまでの残り秒数。0以下なら攻撃可能。
	// Update()で毎フレーム減算する(IsAttackOnCooldown()/NotifyAttackCompleted()参照)。
	float attackCooldownTimer_ = 0.0f;

	size_t patrolIndex_ = 0;

	// ヒステリシス付き索敵状態。detectionRangeで捕捉し、loseTargetRangeより
	// 大きく離れるまでは捕捉状態を維持する(境界上でのChase/Patrol往復を防ぐ)。
	bool hasTarget_ = false;
	TransformComponent* targetTransform_ = nullptr;

	bool isDead_ = false;
	float despawnTimer_ = 0.0f;

	ScopedSubscriber collisionSubscriber_;
	ScopedSubscriber parriedSubscriber_;
	ScopedSubscriber diedSubscriber_;

	std::unique_ptr<IBTNode<EnemyAIController>> root_;
};