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
#include "../Common/ISyncroAnimation.h"

#include "../../../Physics/Movement/MovementComponent.h"
#include "../../../Physics/Collision/ColliderComponent.h"
#include "../../../Graphics/Animation/FacingDirectionComponent.h"
#include "../../../Graphics/Animation/ModelAnimatorComponent.h"
#include "../../../Graphics/Effect/WindShellComponent.h"

#include "Application/Core/EventBus/Events/CombatEvents.h"
#include "Application/Core/EventBus/Events/DebugEvents.h"

// 効果音ID(論理IDまたはファイルパス)。空なら鳴らさない。
// 被弾/ブロックのSEは攻撃側のAttackVisualData(SEName)で指定する。
struct EnemySoundData
{
	std::string attackSwing;	// 攻撃判定の発生時
	std::string roar = "Asset/Sounds/Game/Roar.wav";	// 咆哮(Behaviorから使う)
	std::string death;
};

// 敵AIの本体。BTを駆動し、武器・被弾・アニメ等のコンポーネントへの窓口になる。
// 被弾処理はHitReactionComponentへ委譲し、IsGuarding()はRecovery(隙)以外をハイパーアーマー扱いにする。
// Prefab生成はComponentRegistrations.cppのBuildEnemyAIControllerが担当する
// (ctor引数が多くT::Config+Add<T>()方式に乗らないため)。
class EnemyAIController : public ComponentBase, public IMovementSource, public IHitReactionQuery, public ISyncroAnimation
{
public:
	EnemyAIController(GameObject* owner, const EnemyAIData& data, std::unique_ptr<IEnemyBehavior> behavior);

	// --- ライフサイクル ---
	void Awake() override;
	void Update(float deltaTime) override;

	// --- IMovementSource ---
	Math::Vector3 GetDesiredVelocity() override;

	// --- IHitReactionQuery ---
	bool IsGuarding() const override;
	bool IsInParryWindow() const override;
	void NotifyParrySuccess() override;
	void NotifyGuardHit() override;
	void EnterStagger(bool isLarge) override;

	// --- ISyncroAnimation ---
	void NotifyRethalAnimation() override;

	// --- データ/Blackboard ---
	const EnemyAIData& GetData() const;
	// BTノードへ公開する実行時状態の置き場(値の計算はこのクラスが行う)。
	Blackboard& GetBlackboard();
	const Blackboard& GetBlackboard() const;

	Math::Vector3 GetPosition() const;

	// --- 索敵 ---
	// Blackboardの値を読むだけのアクセサ。
	bool HasTarget() const;
	float DistanceToTarget() const;
	bool IsTargetInAttackRange() const;
	bool IsTargetInGapCloserRange() const;
	Math::Vector3 GetTargetPositionOrSelf() const;

	// --- 技選択 ---
	const EnemyAttackDefinition* ChooseAttack() const;
	const EnemyAttackDefinition* ChooseGapCloserAttack() const;
	// currentのcomboLinksから次の技を間合い/確率で解決する(無ければnullptr)。
	const EnemyAttackDefinition* ResolveComboNext(const EnemyAttackDefinition& current) const;

	// --- 攻撃クールダウン ---
	bool IsAttackOnCooldown() const;
	// intervalAfterが負ならEnemyAIData::attackIntervalDurationを使う。
	void NotifyAttackCompleted(float intervalAfter = -1.0f);

	// --- 巡回 ---
	Math::Vector3 GetCurrentPatrolPoint() const;
	void AdvanceToNextPatrolPoint();

	// --- 移動 ---
	// velocityには正規化済みの方向のみ渡す。大きさはSetMovementSpeed()で設定する。
	void SetDesiredVelocity(const Math::Vector3& velocity);
	void StopMovement();
	void SetMovementSpeed(float speed);

	// --- 向き ---
	void FaceHorizontalTarget(const Math::Vector3& targetPosition);
	// 毎フレーム呼んで目標方向を更新する。FacingDirectionComponentが無ければ瞬時に正対する。
	void FaceTargetSmoothly(const Math::Vector3& targetPosition, float turnSpeed);
	// FaceTargetSmoothly()の追従を止め、移動方向への自動追従へ戻す。
	void StopFacingTarget();

	// --- アニメーション ---
	// 同じアニメを毎フレーム再生し直さないためのラッパー。
	void PlayAnimationIfChanged(const std::string& name, bool loop);
	void PlayAnimation(const std::string& name, bool loop = false, float targetDurationSeconds = -1.0f, bool useRootMotion = false);
	void PlayAnimation(const MotionClipData& clip);

	// --- 武器 ---
	// weaponSet_取得前(Awake前)に呼ばれた場合はpendingWeapons_へ溜め、Awake()で登録する。
	void SetWeapon(const std::string& slot, Handle<WeaponComponent> weapon);
	void SetWeaponHitBoxEnabled(const std::vector<std::string>& slots, bool enabled);
	// Trailを持たない部位では何もしない。
	void SetWeaponTrailEmitting(const std::vector<std::string>& slots, bool emitting);
	// 開始はpresetが空なら何もしない。WindShellComponentの無い部位は無視する。
	void SetWeaponWindEmitting(const std::vector<std::string>& slots, bool emitting, const std::string& preset = "");
	// 攻撃演出データ(SE/エフェクト等)を対象スロットの武器へ渡す。
	void ApplyAttackVisualData(const std::vector<std::string>& slots, const AttackVisualData& visual);
	// Recoveryフェーズ(隙)中はtrue。BTWeightedAttackAction::Tick()が書き換える。
	void SetVulnerable(bool vulnerable);

	// --- 予備動作エフェクト ---
	// convergeDuration秒後に収束しきる追従エフェクトを出す。
	void PlayTelegraphEffect(const EnemyAttackTelegraphData& data, float convergeDuration);
	// 正常終了。エフェクトは寿命まで残し、収束の演出だけ出して追従を終える。
	void FinishTelegraphEffect();
	// 中断。エフェクトを即座に消す。
	void StopTelegraphEffect();

	// --- 効果音 ---
	const EnemySoundData& GetSoundData() const;
	void SetSoundData(const EnemySoundData& sound);
	void PlaySE(const std::string& id) const;
	void PlayAttackSwingSE() const;

	// --- 体幹 ---
	PostureComponent* GetPostureComponent() const;

	// --- 死亡時に道連れで破棄するオブジェクト ---
	void RegisterOwnedObject(Handle<GameObject> obj);

private:
	void UpdateTargetAcquisition();
	void UpdateTelegraphEffect();
	Math::Vector3 ComputeTelegraphPosition() const;
	TransformComponent* FindPlayerTransform() const;
	void OnAttackDeflected(const Events::AttackDeflected& e);
	void OnDied();
	void RequestDespawn();
	void ApplyRootMotion();

	static constexpr const char* kMainWeaponSlot = "Main";

	EnemyAIData data_;
	EnemySoundData sound_;
	std::unique_ptr<IEnemyBehavior> behavior_;
	std::unique_ptr<IBTNode<EnemyAIController>> root_;

	// BTノードから見える実行時状態(HasTarget/DistanceToTarget/IsAttackOnCooldown等)の唯一の実体。
	Blackboard blackboard_;

	TransformComponent* transform_ = nullptr;
	MovementComponent* movementComponent_ = nullptr;
	ModelAnimatorComponent* modelAnimatorComponent_ = nullptr;
	PostureComponent* postureComponent_ = nullptr;
	HealthComponent* healthComponent_ = nullptr;
	FacingDirectionComponent* facingDirectionComponent_ = nullptr;
	WeaponSetComponent* weaponSet_ = nullptr;

	// weaponSet_取得前のSetWeapon()分の一時バッファ。Awake()で登録して空にする。
	std::vector<std::pair<std::string, Handle<WeaponComponent>>> pendingWeapons_;
	std::vector<Handle<GameObject>> ownedObjects_;
	// 風を再生中のスロット(死亡時の停止用)
	std::vector<std::string> windSlots_;

	Math::Vector3 desiredVelocity_{};
	std::string currentAnimationName_;

	float attackCooldownTimer_ = 0.0f;
	bool isVulnerable_ = false;	// Recovery中。IsGuarding()が参照する
	size_t patrolIndex_ = 0;

	// 直前に選んだ技(連続選択の回避用)。const関数内で更新するためmutable。
	mutable const EnemyAttackDefinition* lastAttack_ = nullptr;
	// Blackboardに載せられない内部キャッシュ。
	TransformComponent* targetTransform_ = nullptr;

	bool isDead_ = false;
	float despawnTimer_ = 0.0f;

	// 再生中の予備動作エフェクト(キーが空なら未再生)
	std::string telegraphKey_;
	EnemyAttackTelegraphData telegraphData_;

	ScopedSubscriber parrySucceededSubscriber_;
	ScopedSubscriber diedSubscriber_;
	ScopedSubscriber enemyKillSubscriber_;
};