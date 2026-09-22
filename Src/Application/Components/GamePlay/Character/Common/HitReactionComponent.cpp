#include "HitReactionComponent.h"
#include "../Common/CharacterEvents.h"
#include "WeaponComponent.h"

void HitReactionComponent::Awake()
{
	postureComponent_ = GetOwner()->GetComponent<PostureComponent>();
	healthComponent_ = GetOwner()->GetComponent<HealthComponent>();
	velocityComponent_ = GetOwner()->GetComponent<VelocityComponent>();
	transform_ = GetOwner()->GetComponent<TransformComponent>();

	EventBus& localBus = GetOwner()->GetLocalEventBus();
	const SubscriptionId subscriptionId = localBus.Subscribe<Events::Collision::CollisionEnterEvent>(
		[this](const Events::Collision::CollisionEnterEvent& e) { OnCollisionEnter(e); });
	subscriber_ = ScopedSubscriber(&localBus, subscriptionId);
}

Math::Vector3 HitReactionComponent::ComputeKnockbackDirection(GameObject* attacker) const
{
	Math::Vector3 dir = (transform_ != nullptr) ? transform_->GetForward() : Math::Vector3::Forward;

	if (attacker == nullptr || transform_ == nullptr) return dir;

	if (TransformComponent* attackerTransform = attacker->GetComponent<TransformComponent>()) {
		dir = transform_->GetPosition() - attackerTransform->GetPosition();
		dir.y = 0.0f;
		if (dir.LengthSquared() < 1e-6f) {
			dir = transform_->GetForward();
		}
		else {
			dir.Normalize();
		}
	}
	return dir;
}

void HitReactionComponent::OnCollisionEnter(const Events::Collision::CollisionEnterEvent& e)
{
	if (query_ == nullptr) return;
	if (e.selfShapeName != "HurtBox") return;

	AttackSourceComponent* attack = e.otherObject->GetComponent<AttackSourceComponent>();
	if (attack == nullptr) return;

	// 多段ヒット防止。
	if (attack->alreadyHit.count(GetOwner()) > 0) return;
	attack->alreadyHit.insert(GetOwner());

	GameObject* attacker = attack->ownerCharacter.Resolve();
	const AttackDamageData& attackData = attack->GetAttackDamageData();

	if (query_->IsInParryWindow()) {
		// パリィ成立: 攻撃側の体幹を削り、パリィされた通知を送る。
		if (attacker != nullptr) {
			if (PostureComponent* attackerPosture = attacker->GetComponent<PostureComponent>()) {
				attackerPosture->AddPostureDamage(attackData.parryPostureDamage);
			}
			attacker->GetLocalEventBus().Publish(AttackSourceComponent::ParriedEvent{});
		}
		if (config_.effectFlags.has(HitReactionFlags::WeaponClashFx)) {
			SpawnWeaponClashEffect(e.otherObject, /*isParry=*/true);
		}

		// 自分自身(パリィした側)にも成立を通知
		query_->NotifyParrySuccess();
	}
	else if (query_->IsGuarding()) {
		// 通常ブロック: 自分の体幹を削り、HPにも軽減済みのチップダメージを与える。
		if (config_.effectFlags.has(HitReactionFlags::WeaponClashFx)) {
			SpawnWeaponClashEffect(e.otherObject, /*isParry=*/false);
		}

		if (postureComponent_ != nullptr) {
			postureComponent_->AddPostureDamage(attackData.postureDamage);
			if (postureComponent_->IsBroken()) {
				// TODO: 崩し状態(専用State)への遷移は別途実装。
			}
		}
		if (healthComponent_ != nullptr) {
			healthComponent_->TakeDamage(attackData.damage * attackData.chipDamageRatio);
		}
		// ガード時でもノックバックする。
		if (velocityComponent_ != nullptr) {
			velocityComponent_->AddImpulse(ComputeKnockbackDirection(attacker) * config_.guardKnockbackPower);
		}

		query_->NotifyGuardHit();
	}
	else {
		// 通常被弾: ダメージ+体幹ダメージ+ノックバックを付与し、
		// 体幹が壊れたかどうかで小さい/大きい反応に振り分ける。
		if (healthComponent_ != nullptr) {
			healthComponent_->TakeDamage(attackData.damage);
		}

		if (transform_ != nullptr) {
			if (SceneContext* context = GetOwner()->GetContext()) {
				if (context->eventBus != nullptr) {
					SpawnDamageEffect(e.selfObject, e.otherObject);
				}
			}
		}

		if (velocityComponent_ != nullptr) {
			velocityComponent_->AddImpulse(ComputeKnockbackDirection(attacker) * attackData.knockbackPower);
		}

		// 通常被弾でも体幹にダメージを蓄積する(ガード時とは異なり全ダメージ分)。
		bool postureBroken = false;
		if (postureComponent_ != nullptr) {
			postureComponent_->AddPostureDamage(attackData.postureDamage);
			postureBroken = postureComponent_->IsBroken();
			if (postureBroken) {
				postureComponent_->Reset();
			}
		}

		if (config_.effectFlags.has(HitReactionFlags::HitStop)) {
			PublishHitStop(*GetOwner()->GetContext()->eventBus, config_.hitStopDelaySeconds, config_.hitStopDurationSeconds);
		}
		if (config_.effectFlags.has(HitReactionFlags::CameraShake)) {
			PublishCameraShake(*GetOwner()->GetContext()->eventBus, config_.cameraShakeIntensity);
		}

		query_->EnterStagger(postureBroken, postureBroken ? config_.largeStaggerDuration : attackData.hitStunSeconds);
	}
}

void HitReactionComponent::SpawnWeaponClashEffect(GameObject* attackerWeaponObj, bool isParry)
{
	if (attackerWeaponObj == nullptr) return;

	TransformComponent* attackerWeaponTransform = attackerWeaponObj->GetComponent<TransformComponent>();
	if (attackerWeaponTransform == nullptr) return;

	WeaponComponent* myWeapon = weapon_.Resolve();
	if (myWeapon == nullptr) return;

	ColliderComponent* myWeaponCollider = myWeapon->GetCollider();
	if (myWeaponCollider == nullptr) return;

	TransformComponent* myWeaponTransform = myWeapon->GetTransform();
	if (myWeaponTransform == nullptr) return;

	SceneContext* context = GetOwner()->GetContext();
	if (context == nullptr || context->eventBus == nullptr) return;

	const Math::Vector3 clashPos =
		(attackerWeaponTransform->GetPosition() + myWeaponTransform->GetPosition()) * 0.5f;

	// 両武器の進行方向の差分を、火花が飛び散る基準方向として採用する簡易実装
	// (正確な反射方向の計算はせず、それっぽく見える近似で済ませている。以前はEffectDispatcher::
	//  OnWeaponClash()側にあった計算だが、鍔迫り合いの火花も通常のGenericEffectSpawnEventの
	//  1つとして扱うことにしたため、Publish側であるここに移した)
	Math::Vector3 baseDir = myWeaponTransform->GetForward() - attackerWeaponTransform->GetForward();
	if (baseDir.LengthSquared() < 0.0001f) {
		// 方向が定まらない(ほぼ同じ向き)場合は上向きにフォールバック
		baseDir = Math::Vector3(0.0f, 1.0f, 0.0f);
	}
	baseDir.Normalize();

	const std::string id = isParry ? config_.parryEffectName : config_.blockEffectName;
	PublishGenericEffect(*context->eventBus, id, clashPos, baseDir);
}

void HitReactionComponent::SpawnDamageEffect(GameObject* self, GameObject* attackerWeaponObj)
{
	if (attackerWeaponObj == nullptr) return;

	TransformComponent* attackerWeaponTransform = attackerWeaponObj->GetComponent<TransformComponent>();
	if (attackerWeaponTransform == nullptr) return;

	TransformComponent* myTransform = self->GetComponent<TransformComponent>();
	if (myTransform == nullptr) return;

	SceneContext* context = GetOwner()->GetContext();
	if (context == nullptr || context->eventBus == nullptr) return;

	const Math::Vector3 clashPos =
		(attackerWeaponTransform->GetPosition() + myTransform->GetPosition()) * 0.5f;

	PublishGenericEffect(*context->eventBus, config_.damageEffectName, clashPos);
}