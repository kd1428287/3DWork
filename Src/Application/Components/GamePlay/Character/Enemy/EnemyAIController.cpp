#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include "EnemyAIController.h"
#include "../Common/CharacterEvents.h"
#include "Application/Core/EventBus/Events/AudioEvents.h"

namespace
{
	// 0〜1の乱数(std::rand使用。要見直し)
	float Random01()
	{
		return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	}

	// poolからpredを満たす技を重み付き抽選する。該当なしならnullptr。
	template <typename Predicate>
	const EnemyAttackDefinition* WeightedPick(const std::vector<EnemyAttackDefinition>& pool, Predicate pred)
	{
		float totalWeight = 0.0f;
		for (const auto& atk : pool) {
			if (pred(atk)) totalWeight += atk.weight;
		}
		if (totalWeight <= 0.0f) return nullptr;

		float roll = Random01() * totalWeight;
		for (const auto& atk : pool) {
			if (!pred(atk)) continue;
			roll -= atk.weight;
			if (roll <= 0.0f) return &atk;
		}

		// 浮動小数の誤差で抜けた場合は最初の候補を返す
		for (const auto& atk : pool) {
			if (pred(atk)) return &atk;
		}
		return nullptr;
	}

	// 直前の技(exclude)を除いて抽選する。除くと候補が無くなる場合は除外しない。
	const EnemyAttackDefinition* ChooseFrom(const std::vector<EnemyAttackDefinition>& pool, float dist,
		const EnemyAttackDefinition* exclude)
	{
		auto inRange = [dist](const EnemyAttackDefinition& atk) {
			return atk.isEntry && dist >= atk.minRange && dist <= atk.maxRange;
			};

		const EnemyAttackDefinition* picked = WeightedPick(pool,
			[&](const EnemyAttackDefinition& atk) { return inRange(atk) && &atk != exclude; });
		return picked != nullptr ? picked : WeightedPick(pool, inRange);
	}

	// 間合い内に入っている技が1つでもあるか
	bool IsInAnyRange(const std::vector<EnemyAttackDefinition>& pool, float dist)
	{
		for (const auto& atk : pool) {
			if (dist >= atk.minRange && dist <= atk.maxRange) return true;
		}
		return false;
	}
}

// --- 生成・ライフサイクル -------------------------------------------------
EnemyAIController::EnemyAIController(GameObject* owner, const EnemyAIData& data, std::unique_ptr<IEnemyBehavior> behavior)
	: ComponentBase(owner), data_(data), behavior_(std::move(behavior))
{}

void EnemyAIController::Awake()
{
	assert(data_.ValidateAttacks() && "EnemyAIData: attack idの重複、またはcomboLinksの参照先不在");

	GameObject* owner = GetOwner();
	transform_ = owner->GetComponent<TransformComponent>();
	movementComponent_ = owner->GetComponent<MovementComponent>();
	modelAnimatorComponent_ = owner->GetComponent<ModelAnimatorComponent>();
	postureComponent_ = owner->GetComponent<PostureComponent>();
	healthComponent_ = owner->GetComponent<HealthComponent>();
	weaponSet_ = owner->GetComponent<WeaponSetComponent>();
	facingDirectionComponent_ = owner->GetComponent<FacingDirectionComponent>();	// 任意

	// Awake前にSetWeapon()で溜めた武器をここで登録する
	if (weaponSet_ != nullptr) {
		for (auto& [slot, weapon] : pendingWeapons_) {
			weaponSet_->RegisterWeapon(slot, weapon);
		}
		pendingWeapons_.clear();
	}

	if (movementComponent_ != nullptr) {
		movementComponent_->SetMovementSource(this);
	}

	// 被弾処理の問い合わせ先に自分を登録し、鍔迫り合い演出用にメイン武器も渡す
	if (HitReactionComponent* hitReaction = owner->GetComponent<HitReactionComponent>()) {
		hitReaction->SetQuerySource(this);
		if (weaponSet_ != nullptr) {
			if (WeaponComponent* mainWeapon = weaponSet_->GetWeapon(kMainWeaponSlot)) {
				hitReaction->SetWeapon(Handle<WeaponComponent>(mainWeapon));
			}
		}
	}

	EventBus& localBus = owner->GetLocalEventBus();

	if (healthComponent_ != nullptr) {
		diedSubscriber_ = localBus.SubscribeScoped<Events::Died>(
			[this](const Events::Died&) { OnDied(); });
	}

	parrySucceededSubscriber_ = localBus.SubscribeScoped<Events::AttackDeflected>(
		[this](const Events::AttackDeflected& e) { OnAttackDeflected(e); });

	enemyKillSubscriber_ = GLOBALEVENT->SubscribeScoped<Events::EnemyKill>(
		[this](const Events::EnemyKill&) {
			if (healthComponent_ != nullptr) healthComponent_->SetCurrent(0);
		});

	root_ = behavior_->BuildTree(this);
	behavior_->OnSpawned(this);
}

void EnemyAIController::Update(float deltaTime)
{
	if (isDead_) {
		despawnTimer_ -= deltaTime;
		if (despawnTimer_ <= 0.0f) RequestDespawn();
		return;
	}

	if (attackCooldownTimer_ > 0.0f) {
		attackCooldownTimer_ = std::max(attackCooldownTimer_ - deltaTime, 0.0f);
	}

	// BTがTick()で参照できるよう、先にBlackboardへ状態を書き込む
	blackboard_.SetBool(EnemyBlackboardKeys::IsAttackOnCooldown, attackCooldownTimer_ > 0.0f);

	UpdateTargetAcquisition();
	blackboard_.SetBool(EnemyBlackboardKeys::IsTargetInAttackRange, IsTargetInAttackRange());
	blackboard_.SetBool(EnemyBlackboardKeys::IsTargetInGapCloserRange, IsTargetInGapCloserRange());

	if (root_ != nullptr) root_->Tick(this, deltaTime);

	UpdateTelegraphEffect();
	ApplyRootMotion();
}

// --- IMovementSource ------------------------------------------------------
Math::Vector3 EnemyAIController::GetDesiredVelocity()
{
	return desiredVelocity_;
}

// --- IHitReactionQuery ----------------------------------------------------
bool EnemyAIController::IsGuarding() const
{
	return !isVulnerable_;
}

bool EnemyAIController::IsInParryWindow() const
{
	return false;
}

void EnemyAIController::NotifyParrySuccess()
{}

void EnemyAIController::NotifyGuardHit()
{}

void EnemyAIController::EnterStagger(bool isLarge)
{
	// スタン時間はBehavior側で決めるためdurationは0を渡す
	if (behavior_) behavior_->OnStaggered(this, isLarge, 0.0f);
}

// --- ISyncroAnimation -----------------------------------------------------
void EnemyAIController::NotifyRethalAnimation()
{
	if (behavior_) behavior_->OnFatalized(this);
}

// --- データ/Blackboard ----------------------------------------------------
const EnemyAIData& EnemyAIController::GetData() const
{
	return data_;
}

Blackboard& EnemyAIController::GetBlackboard()
{
	return blackboard_;
}

const Blackboard& EnemyAIController::GetBlackboard() const
{
	return blackboard_;
}

Math::Vector3 EnemyAIController::GetPosition() const
{
	return transform_ != nullptr ? transform_->GetPosition() : Math::Vector3::Zero;
}

// --- 索敵 -----------------------------------------------------------------
bool EnemyAIController::HasTarget() const
{
	return blackboard_.GetBoolOr(EnemyBlackboardKeys::HasTarget, false);
}

float EnemyAIController::DistanceToTarget() const
{
	return blackboard_.GetFloatOr(EnemyBlackboardKeys::DistanceToTarget, FLT_MAX);
}

bool EnemyAIController::IsTargetInAttackRange() const
{
	return HasTarget() && IsInAnyRange(data_.attacks, DistanceToTarget());
}

bool EnemyAIController::IsTargetInGapCloserRange() const
{
	return HasTarget() && IsInAnyRange(data_.gapCloserAttacks, DistanceToTarget());
}

Math::Vector3 EnemyAIController::GetTargetPositionOrSelf() const
{
	if (HasTarget() && targetTransform_ != nullptr) return targetTransform_->GetPosition();
	return GetPosition();
}

void EnemyAIController::UpdateTargetAcquisition()
{
	targetTransform_ = FindPlayerTransform();
	if (targetTransform_ == nullptr || transform_ == nullptr) {
		blackboard_.SetBool(EnemyBlackboardKeys::HasTarget, false);
		blackboard_.SetFloat(EnemyBlackboardKeys::DistanceToTarget, FLT_MAX);
		return;
	}

	const float distSq = (targetTransform_->GetPosition() - transform_->GetPosition()).LengthSquared();

	// ヒステリシス: 捕捉はdetectionRange、解除はloseTargetRangeで判定する
	const bool wasTracking = blackboard_.GetBoolOr(EnemyBlackboardKeys::HasTarget, false);
	const float range = wasTracking ? data_.loseTargetRange : data_.detectionRange;
	const bool hasTarget = distSq <= range * range;

	blackboard_.SetBool(EnemyBlackboardKeys::HasTarget, hasTarget);
	blackboard_.SetFloat(EnemyBlackboardKeys::DistanceToTarget, std::sqrt(distSq));
}

TransformComponent* EnemyAIController::FindPlayerTransform() const
{
	const SceneContext* context = GetOwner()->GetContext();
	if (context == nullptr) return nullptr;

	// SceneContext::player(PlayerComponentが自己登録する弱参照)経由で取得する
	GameObject* player = context->player.Resolve();
	return (player != nullptr) ? player->GetComponent<TransformComponent>() : nullptr;
}

// --- 技選択 ---------------------------------------------------------------
const EnemyAttackDefinition* EnemyAIController::ChooseAttack() const
{
	if (!HasTarget()) return nullptr;
	lastAttack_ = ChooseFrom(data_.attacks, DistanceToTarget(), lastAttack_);
	return lastAttack_;
}

const EnemyAttackDefinition* EnemyAIController::ChooseGapCloserAttack() const
{
	if (!HasTarget()) return nullptr;
	lastAttack_ = ChooseFrom(data_.gapCloserAttacks, DistanceToTarget(), lastAttack_);
	return lastAttack_;
}

const EnemyAttackDefinition* EnemyAIController::ResolveComboNext(const EnemyAttackDefinition& current) const
{
	if (!HasTarget()) return nullptr;
	const float dist = DistanceToTarget();

	// priorityの小さい順に評価する
	std::vector<const EnemyComboLink*> links;
	links.reserve(current.comboLinks.size());
	for (const auto& link : current.comboLinks) links.push_back(&link);
	std::stable_sort(links.begin(), links.end(),
		[](const EnemyComboLink* a, const EnemyComboLink* b) { return a->priority < b->priority; });

	for (const EnemyComboLink* link : links) {
		if (dist < link->minRange || dist > link->maxRange) continue;
		if (link->chance < 1.0f && Random01() > link->chance) continue;
		if (const EnemyAttackDefinition* next = data_.FindAttack(link->nextAttackId)) return next;
	}
	return nullptr;
}

// --- 攻撃クールダウン -----------------------------------------------------
bool EnemyAIController::IsAttackOnCooldown() const
{
	return blackboard_.GetBoolOr(EnemyBlackboardKeys::IsAttackOnCooldown, false);
}

void EnemyAIController::NotifyAttackCompleted(float intervalAfter)
{
	attackCooldownTimer_ = (intervalAfter >= 0.0f) ? intervalAfter : data_.attackIntervalDuration;
}

// --- 巡回 -----------------------------------------------------------------
Math::Vector3 EnemyAIController::GetCurrentPatrolPoint() const
{
	if (data_.patrolPoints.empty()) return GetPosition();
	return data_.patrolPoints[patrolIndex_ % data_.patrolPoints.size()];
}

void EnemyAIController::AdvanceToNextPatrolPoint()
{
	if (data_.patrolPoints.empty()) return;
	patrolIndex_ = (patrolIndex_ + 1) % data_.patrolPoints.size();
}

// --- 移動 -----------------------------------------------------------------
void EnemyAIController::SetDesiredVelocity(const Math::Vector3& velocity)
{
	desiredVelocity_ = velocity;
}

void EnemyAIController::StopMovement()
{
	desiredVelocity_ = Math::Vector3::Zero;
}

void EnemyAIController::SetMovementSpeed(float speed)
{
	if (movementComponent_ != nullptr) movementComponent_->SetSpeed(speed);
}

void EnemyAIController::ApplyRootMotion()
{
	if (modelAnimatorComponent_ == nullptr || transform_ == nullptr) return;

	const Math::Vector3 localDelta = modelAnimatorComponent_->ConsumeRootMotionDelta();
	if (localDelta.LengthSquared() <= 0.0f) return;

	transform_->Translate(Math::Vector3::Transform(localDelta, transform_->GetRotation()));
}

// --- 向き -----------------------------------------------------------------
void EnemyAIController::FaceHorizontalTarget(const Math::Vector3& targetPosition)
{
	if (transform_ == nullptr) return;

	Math::Vector3 dir = targetPosition - transform_->GetPosition();
	dir.y = 0.0f;
	if (dir.LengthSquared() < 1e-6f) return;
	dir.Normalize();

	const float yaw = std::atan2(-dir.x, -dir.z);
	transform_->SetRotation(Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yaw));
}

void EnemyAIController::FaceTargetSmoothly(const Math::Vector3& targetPosition, float turnSpeed)
{
	if (transform_ == nullptr) return;
	if (facingDirectionComponent_ == nullptr) {
		FaceHorizontalTarget(targetPosition);
		return;
	}

	Math::Vector3 dir = targetPosition - transform_->GetPosition();
	dir.y = 0.0f;
	if (dir.LengthSquared() < 1e-6f) return;
	dir.Normalize();

	// 移動アニメで自動追従が止まったままの場合があるため、明示的に有効へ戻す
	facingDirectionComponent_->SetUpdateEnabled(true);
	facingDirectionComponent_->SetTargetOverrideDirection(dir, turnSpeed);
}

void EnemyAIController::StopFacingTarget()
{
	if (facingDirectionComponent_ != nullptr) {
		facingDirectionComponent_->ClearTargetOverrideDirection();
	}
}

// --- アニメーション -------------------------------------------------------
void EnemyAIController::PlayAnimationIfChanged(const std::string& name, bool loop)
{
	if (name == currentAnimationName_) return;
	PlayAnimation(name, loop);
}

void EnemyAIController::PlayAnimation(const std::string& name, bool loop, float targetDurationSeconds, bool useRootMotion)
{
	currentAnimationName_ = name;
	if (modelAnimatorComponent_ != nullptr) {
		modelAnimatorComponent_->SetRootMotionEnabled(useRootMotion);
		modelAnimatorComponent_->Play(name, loop, targetDurationSeconds);
	}
	// ルートモーション中は向きの自動追従を止める
	if (facingDirectionComponent_ != nullptr) {
		facingDirectionComponent_->SetUpdateEnabled(!useRootMotion);
	}
}

void EnemyAIController::PlayAnimation(const MotionClipData& clip)
{
	currentAnimationName_ = clip.animationName;
	if (modelAnimatorComponent_ == nullptr) return;
	modelAnimatorComponent_->Play(clip);
}

// --- 武器 -----------------------------------------------------------------
void EnemyAIController::SetWeapon(const std::string& slot, Handle<WeaponComponent> weapon)
{
	if (weaponSet_ != nullptr) {
		weaponSet_->RegisterWeapon(slot, weapon);
	}
	else {
		// Prefab生成時はAwake前に呼ばれるため、一時バッファへ溜める
		pendingWeapons_.emplace_back(slot, weapon);
	}
}

void EnemyAIController::SetWeaponHitBoxEnabled(const std::vector<std::string>& slots, bool enabled)
{
	if (weaponSet_ != nullptr) weaponSet_->SetHitBoxEnabled(slots, enabled);
}

void EnemyAIController::SetWeaponTrailEmitting(const std::vector<std::string>& slots, bool emitting)
{
	if (weaponSet_ != nullptr) weaponSet_->SetTrailEmitting(slots, emitting);
}

void EnemyAIController::SetWeaponWindEmitting(const std::vector<std::string>& slots, bool emitting, const std::string& preset)
{
	if (weaponSet_ == nullptr) return;
	if (emitting && preset.empty()) return;

	for (const std::string& slot : slots) {
		WeaponComponent* weapon = weaponSet_->GetWeapon(slot);
		if (weapon == nullptr) continue;

		WindShellComponent* wind = weapon->GetOwner()->GetComponent<WindShellComponent>();
		if (wind == nullptr) continue;

		if (emitting) {
			wind->SetPreset(preset);
			wind->Play();
		}
		else {
			wind->Stop();
		}
	}

	if (emitting) windSlots_ = slots;
	else windSlots_.clear();
}

void EnemyAIController::ApplyAttackVisualData(const std::vector<std::string>& slots, const AttackVisualData& visual)
{
	if (weaponSet_ == nullptr) return;
	for (const std::string& slot : slots) {
		if (WeaponComponent* weapon = weaponSet_->GetWeapon(slot)) {
			weapon->SetAttackVisualData(visual);
		}
	}
}

void EnemyAIController::SetVulnerable(bool vulnerable)
{
	isVulnerable_ = vulnerable;
}

// --- 予備動作エフェクト ---------------------------------------------------
void EnemyAIController::PlayTelegraphEffect(const EnemyAttackTelegraphData& data, float convergeDuration)
{
	if (data.effectName.empty() || transform_ == nullptr) return;

	StopTelegraphEffect();	// 二重再生防止
	telegraphData_ = data;
	telegraphKey_ = "EnemyTelegraph_" + std::to_string(reinterpret_cast<uintptr_t>(this));

	// 粒子の発生を収束時間の何割へばらけさせるか(収束時刻は全粒子で共通)
	constexpr float kTelegraphSpawnSpread = 0.7f;
	Events::PublishEffectFollowSpawn(GetOwner()->GetSceneEventBus(), telegraphKey_, data.effectName,
		ComputeTelegraphPosition(), convergeDuration, kTelegraphSpawnSpread);
}

void EnemyAIController::UpdateTelegraphEffect()
{
	if (telegraphKey_.empty() || transform_ == nullptr) return;
	Events::PublishEffectPositionUpdate(GetOwner()->GetSceneEventBus(), telegraphKey_, ComputeTelegraphPosition());
}

void EnemyAIController::FinishTelegraphEffect()
{
	if (telegraphKey_.empty()) return;
	// 追従エフェクトは寿命で自動破棄されるためDetachしない
	Events::PublishGenericEffect(GetOwner()->GetSceneEventBus(), std::string("Star"), ComputeTelegraphPosition());
	telegraphKey_.clear();
}

void EnemyAIController::StopTelegraphEffect()
{
	if (telegraphKey_.empty()) return;
	Events::PublishEffectDetach(GetOwner()->GetSceneEventBus(), telegraphKey_);
	telegraphKey_.clear();
}

Math::Vector3 EnemyAIController::ComputeTelegraphPosition() const
{
	// 基準はanchorSlotの武器位置。見つからなければルート位置
	Math::Vector3 basePos = transform_->GetPosition();
	if (!telegraphData_.anchorSlot.empty() && weaponSet_ != nullptr) {
		if (WeaponComponent* weapon = weaponSet_->GetWeapon(telegraphData_.anchorSlot)) {
			if (TransformComponent* weaponTf = weapon->GetTransform()) {
				basePos = weaponTf->GetPosition();
			}
		}
	}
	// offsetは敵の向きに合わせて回す
	return basePos + Math::Vector3::Transform(telegraphData_.offset, transform_->GetRotation());
}

// --- 効果音 ---------------------------------------------------------------
const EnemySoundData& EnemyAIController::GetSoundData() const
{
	return sound_;
}

void EnemyAIController::SetSoundData(const EnemySoundData& sound)
{
	sound_ = sound;
}

void EnemyAIController::PlaySE(const std::string& id) const
{
	if (id.empty()) return;
	Events::PublishSoundPlayEvent(*GlobalEventBus::Instance(), id);
}

void EnemyAIController::PlayAttackSwingSE() const
{
	PlaySE(sound_.attackSwing);
}

// --- 体幹 -----------------------------------------------------------------
PostureComponent* EnemyAIController::GetPostureComponent() const
{
	return postureComponent_;
}

// --- 死亡・破棄 -----------------------------------------------------------
void EnemyAIController::RegisterOwnedObject(Handle<GameObject> obj)
{
	ownedObjects_.push_back(obj);
}

void EnemyAIController::OnAttackDeflected(const Events::AttackDeflected& e)
{
	// 現状パリィされても何もしない(Behaviorへの通知は未実装)
}

void EnemyAIController::OnDied()
{
	if (isDead_) return;
	isDead_ = true;

	StopTelegraphEffect();
	SetWeaponWindEmitting(std::vector<std::string>(windSlots_), false);
	despawnTimer_ = behavior_ ? behavior_->GetDespawnDelay(this) : 1.5f;

	PlaySE(sound_.death);

	StopMovement();
	if (movementComponent_ != nullptr) movementComponent_->SetEnabled(false);
	if (ColliderComponent* collider = GetOwner()->GetComponent<ColliderComponent>()) {
		collider->SetEnabled(false);
	}

	if (behavior_) behavior_->OnDied(this);
}

void EnemyAIController::RequestDespawn()
{
	StopTelegraphEffect();

	const SceneContext* context = GetOwner()->GetContext();
	if (context == nullptr || context->objectManager == nullptr) return;

	for (Handle<GameObject>& owned : ownedObjects_) {
		if (GameObject* obj = owned.Resolve()) {
			context->objectManager->Destroy(obj);
		}
	}
	context->objectManager->Destroy(GetOwner());
}