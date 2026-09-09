#include "HitReactionComponent.h"
#include "CharacterEvents.h"

void HitReactionComponent::Awake()
{
	EventBus& localBus = GetOwner()->GetLocalEventBus();
	const SubscriptionId subscriptionId = localBus.Subscribe<CollisionSystem::CollisionEnterEvent>(
		[this](const CollisionSystem::CollisionEnterEvent& e) { OnCollisionEnter(e); });
	subscriber_ = ScopedSubscriber(&localBus, subscriptionId);
}

void HitReactionComponent::Start()
{
	postureComponent_ = GetOwner()->GetComponent<PostureComponent>();
	healthComponent_ = GetOwner()->GetComponent<HealthComponent>();
	velocityComponent_ = GetOwner()->GetComponent<VelocityComponent>();
	transform_ = GetOwner()->GetComponent<TransformComponent>();
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

void HitReactionComponent::OnCollisionEnter(const CollisionSystem::CollisionEnterEvent& e)
{
	if (query_ == nullptr) return;
	if (e.selfShapeName != "HurtBox") return;

	AttackSourceComponent* attack = e.otherObject->GetComponent<AttackSourceComponent>();
	if (attack == nullptr) return;

	// 多段ヒット防止。
	if (attack->alreadyHit.count(GetOwner()) > 0) return;
	attack->alreadyHit.insert(GetOwner());

	GameObject* attacker = attack->ownerCharacter.Resolve();

	if (query_->IsInParryWindow()) {
		// パリィ成立: 攻撃側の体幹を削り、パリィされた通知を送る。
		if (attacker != nullptr) {
			if (PostureComponent* attackerPosture = attacker->GetComponent<PostureComponent>()) {
				attackerPosture->AddPostureDamage(attack->parryPostureDamage);
			}
			attacker->GetLocalEventBus().Publish(AttackSourceComponent::ParriedEvent{});
		}
		SpawnWeaponClashEffect(e.otherObject, /*isParry=*/true);

		// 自分自身(パリィした側)にも成立を通知し、専用の成功モーションを
		// 再生してもらう(State側の演出。StateGuard::NotifyParrySuccess参照)。
		query_->NotifyParrySuccess();
	}
	else if (query_->IsGuarding()) {
		// 通常ブロック: 自分の体幹を削り、HPにも軽減済みのチップダメージを与える。
		SpawnWeaponClashEffect(e.otherObject, /*isParry=*/false);

		if (postureComponent_ != nullptr) {
			postureComponent_->AddPostureDamage(attack->postureDamage);
			if (postureComponent_->IsBroken()) {
				// TODO: 崩し状態(専用State)への遷移は別途実装。
			}
		}
		if (healthComponent_ != nullptr) {
			healthComponent_->TakeDamage(attack->damage * attack->chipDamageRatio);
		}
		// ガード時でもノックバックする。
		if (velocityComponent_ != nullptr) {
			velocityComponent_->AddImpulse(ComputeKnockbackDirection(attacker) * kGuardKnockbackPower);
		}

		// ブロックの都度、ヒットリアクションのアニメーションを再生してもらう
		// (パリィ成功ほど特別な演出ではないため、ガード解除や反撃キャンセルの
		//  可否には影響させない。StateGuard::NotifyGuardHit参照)。
		query_->NotifyGuardHit();
	}
	else {
		// 通常被弾: ダメージ+体幹ダメージ+ノックバックを付与し、
		// 体幹が壊れたかどうかで小さい/大きい反応に振り分ける。
		if (healthComponent_ != nullptr) {
			healthComponent_->TakeDamage(attack->damage);
		}

		if (transform_ != nullptr) {
			if (SceneContext* context = GetOwner()->GetContext()) {
				if (context->eventBus != nullptr) {
					SpawnDamageEffect(e.selfObject, e.otherObject);
				}
			}
		}

		if (velocityComponent_ != nullptr) {
			velocityComponent_->AddImpulse(ComputeKnockbackDirection(attacker) * attack->knockbackPower);
		}

		// 通常被弾でも体幹にダメージを蓄積する(ガード時とは異なり全ダメージ分)。
		bool postureBroken = false;
		if (postureComponent_ != nullptr) {
			postureComponent_->AddPostureDamage(attack->postureDamage);
			postureBroken = postureComponent_->IsBroken();
			if (postureBroken) {
				// TODO: PostureComponent側の実際のリセットAPI名に合わせて修正すること
				// (現状Reset()という名称を仮定している)。
				postureComponent_->Reset();
			}
		}

		RequestHitStopEvent(*GetOwner()->GetContext()->eventBus, 0.f, 0.1f);

		query_->EnterStagger(postureBroken, postureBroken ? largeStaggerDuration_ : attack->hitStunSeconds);
	}
}

// 衝突位置は正確な接触点ではなく、両武器座標の中間点で近似する。
// attackerWeaponObjは相手側の武器GameObject(AttackSourceComponentの持ち主。
// e.otherObjectをそのまま渡す想定)。自分側の武器はweaponCollider_から
// 解決する。双方のTransformComponentが取れない場合、またはシーンバスが
// 取得できない場合は何もしない(片方の武器が既に破棄済み等の異常系)。
void HitReactionComponent::SpawnWeaponClashEffect(GameObject* attackerWeaponObj, bool isParry)
{
	if (attackerWeaponObj == nullptr) return;

	TransformComponent* attackerWeaponTransform = attackerWeaponObj->GetComponent<TransformComponent>();
	if (attackerWeaponTransform == nullptr) return;

	ColliderComponent* myWeaponCollider = weaponCollider_.Resolve();
	if (myWeaponCollider == nullptr) return;

	TransformComponent* myWeaponTransform = myWeaponCollider->GetOwner()->GetComponent<TransformComponent>();
	if (myWeaponTransform == nullptr) return;

	SceneContext* context = GetOwner()->GetContext();
	if (context == nullptr || context->eventBus == nullptr) return;

	const Math::Vector3 clashPos =
		(attackerWeaponTransform->GetPosition() + myWeaponTransform->GetPosition()) * 0.5f;

	PublishWeaponClashEffect(*context->eventBus, clashPos,
		myWeaponTransform->GetForward(), attackerWeaponTransform->GetForward(), isParry);
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

	PublishGenericEffect(*context->eventBus, "BloodSplatter", clashPos);
}