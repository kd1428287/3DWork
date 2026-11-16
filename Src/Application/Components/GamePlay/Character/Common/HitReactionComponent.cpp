#include "HitReactionComponent.h"
#include "../Common/CharacterEvents.h"
#include <algorithm>
#include <random>
#include "Application/Core/EventBus/Events/AudioEvents.h"
#include "../../../Tags/IHitReactionQuery.h"


namespace
{
	// 武器の判定形状名(WeaponComponent::SetHitBoxEnabledと同じ)。
	constexpr const char* kWeaponShapeName = "HitBox";

	// SEのピッチ揺らぎ。-range〜+rangeの一様乱数を返す(range<=0なら0)。
	float RandomPitchOffset(float range)
	{
		if (range <= 0.0f) return 0.0f;

		static thread_local std::mt19937 s_rng{ std::random_device{}() };
		return std::uniform_real_distribution<float>(-range, range)(s_rng);
	}
}

void HitReactionComponent::Awake()
{
	postureComponent_ = GetOwner()->GetComponent<PostureComponent>();
	healthComponent_ = GetOwner()->GetComponent<HealthComponent>();
	transform_ = GetOwner()->GetComponent<TransformComponent>();

	subscriber_ = GetOwner()->GetLocalEventBus().SubscribeScoped<Events::AttackHit>([this](const Events::AttackHit& e) {OnHit(e); });
}

// ============================================================
// 裁定+適用
// ============================================================

void HitReactionComponent::OnHit(const Events::AttackHit& ctx)
{
	if (!Accept(ctx)) return;

	const HitJudgeState state{ query_->IsInParryWindow(), query_->IsGuarding() };
	const HitOutcome outcome = ResolveHit(state, ctx.damageData);
	const ApplyResult result = Apply(outcome);

	const HitResolved resolved = BuildResolved(outcome, result, ctx);
	NotifyAttacker(outcome, resolved);
	PlayFeedback(resolved);
	Dispatch(outcome, result);
}

bool HitReactionComponent::Accept(const Events::AttackHit& ctx)
{
	if (query_ == nullptr) return false;

	// 死亡済みなら、二重の死亡処理や演出を避けるため無視する。
	if (healthComponent_ != nullptr && healthComponent_->IsDead()) return false;

	AttackSourceComponent* attackSource = ctx.attackSource.Resolve();
	if (attackSource == nullptr) return false;

	const Handle<GameObject> self(GetOwner());
	if (attackSource->IsAlreadyHit(self)) return false;
	attackSource->Hit(self);
	return true;
}

ApplyResult HitReactionComponent::Apply(const HitOutcome& outcome)
{
	ApplyResult result;

	if (postureComponent_ != nullptr && outcome.postureDamage > 0.0f) {
		postureComponent_->AddPostureDamage(outcome.postureDamage);
		result.postureBroken = postureComponent_->IsBroken();
		if (result.postureBroken) {
			// TODO: 崩し状態(専用State)への遷移は別途実装。
			// 【暫定】専用Stateができるまで、満タン固着を防ぐためResetだけ行う。
			postureComponent_->Reset();
		}
	}

	if (healthComponent_ != nullptr && outcome.healthDamage > 0.0f) {
		healthComponent_->TakeDamage(outcome.healthDamage);
	}
	result.died = (healthComponent_ != nullptr) && healthComponent_->IsDead();

	return result;
}

void HitReactionComponent::NotifyAttacker(const HitOutcome& outcome, const HitResolved& resolved)
{
	if (outcome.type != HitType::Parry) return;

	GameObject* attacker = resolved.attacker.Resolve();
	if (attacker == nullptr) return;

	if (PostureComponent* attackerPosture = attacker->GetComponent<PostureComponent>()) {
		attackerPosture->AddPostureDamage(outcome.attackerPostureDamage);
	}

	Events::AttackDeflected deflected;
	deflected.incomingDir = resolved.incomingDir;
	deflected.reaction = resolved.visual.parry;
	attacker->GetLocalEventBus().Publish(deflected);
}

void HitReactionComponent::Dispatch(const HitOutcome& outcome, const ApplyResult& result)
{
	switch (outcome.type) {
	case HitType::Parry:
		query_->NotifyParrySuccess();
		break;

	case HitType::Block:
		// チップダメージで致死なら、ガード反応で死亡Stateを上書きしない。
		if (result.died) return;
		query_->NotifyGuardHit();
		break;

	case HitType::Hit:
		// 致死ならスタガーに入らない(死亡Stateを上書きしないため)。
		if (result.died) return;
		query_->EnterStagger(result.postureBroken);
		break;
	}
}

HitResolved HitReactionComponent::BuildResolved(
	const HitOutcome& outcome, const ApplyResult& result, const Events::AttackHit& ctx) const
{
	HitResolved r;
	r.type = outcome.type;
	r.healthDamage = outcome.healthDamage;
	r.postureDamage = outcome.postureDamage;
	r.died = result.died;
	r.postureBroken = result.postureBroken;

	r.hitPos = ctx.hitResult.hitPos;
	r.hitNormal = ctx.hitResult.hitNormal;
	r.incomingDir = ComputeIncomingDirection(ctx);

	r.visual = ctx.visualData;
	r.attacker = ctx.attacker;
	r.weaponObject = ctx.weaponObject;
	return r;
}

Math::Vector3 HitReactionComponent::ComputeIncomingDirection(const Events::AttackHit& ctx) const
{
	// 武器の振り方向。取れなければ攻撃者→自分。
	Math::Vector3 incoming = ctx.lastValidSwingDir;
	if (incoming.LengthSquared() < 1e-6f) {
		return ComputeDirectionFromAttacker(ctx.attacker.Resolve());
	}
	incoming.Normalize();
	return incoming;
}

Math::Vector3 HitReactionComponent::ComputeDirectionFromAttacker(GameObject* attacker) const
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

// ============================================================
// 演出(HitResolvedだけを入力にする)
// ============================================================

void HitReactionComponent::PlayFeedback(const HitResolved& r)
{
	const ReactionEventConfig& config = SelectReaction(r);
	EventBus& bus = *GetOwner()->GetContext()->eventBus;

	// 被弾/ブロック/パリィのSE(設定は攻撃側データ、鳴らすのは受ける側)
	if (!config.SEName.empty()) {
		Events::PublishSoundPlayEvent(*GlobalEventBus::Instance(), config.SEName,
			false, 1.0f, RandomPitchOffset(config.sePitchRandom));
	}

	if (config.enableCameraShake) {
		Events::PublishCameraShake(bus, config.cameraShakeIntensity);
	}
	if (config.enableHitStop) {
		Events::PublishHitStop(bus, r.visual.hitStopDelaySeconds, config.hitStopDurationSeconds);
	}
	if (config.enableEffect) {
		// 反射を使う設定なら反射エフェクト、そうでなければ通常エフェクト。
		if (config.useReflectDirection) SpawnReflectEffect(config, r);
		else                            SpawnNormalEffect(config, r);
	}
	if (config.enableDistortion)
	{
		KdShaderManager::Instance().m_postProcessShader.AddShockwaveWorld(r.hitPos, 0.45f, 0.7f, 0.03f);
	}
}

const ReactionEventConfig& HitReactionComponent::SelectReaction(const HitResolved& r) const
{
	switch (r.type) {
	case HitType::Parry: return r.visual.parry;
	case HitType::Block: return r.visual.block;
	default:             return r.visual.hit;
	}
}

void HitReactionComponent::SpawnReflectEffect(const ReactionEventConfig& config, const HitResolved& r)
{
	if (config.effectName.empty()) return;

	// 既定は衝突点と胴体法線。武器情報が取れたら上書きする。
	Math::Vector3 pos = r.hitPos;
	Math::Vector3 normal = ComputeBodyHitNormal(r);

	if (const auto clash = FindWeaponClashPoints(r)) {
		pos = clash->onA;

		// 武器同士が離れていれば、自武器→攻撃側の向きを法線にする。
		Math::Vector3 toAttacker = clash->onB - clash->onA;
		if (toAttacker.LengthSquared() > 1e-6f) {
			toAttacker.Normalize();
			normal = toAttacker;
		}
	}
	else if (WeaponComponent* myWeapon = weapon_.Resolve()) {
		if (TransformComponent* weaponTf = myWeapon->GetTransform()) {
			pos = weaponTf->GetPosition();
		}
	}

	const Math::Vector3 dir = BlendReflectDirection(normal, r.incomingDir, config.normalBlendRatio);
	Events::PublishGenericEffect(GetOwner()->GetSceneEventBus(), config.effectName, pos, dir);
}

void HitReactionComponent::SpawnNormalEffect(const ReactionEventConfig& config, const HitResolved& r)
{
	if (config.effectName.empty()) return;

	const Math::Vector3 normal = ComputeBodyHitNormal(r);
	Events::PublishGenericEffect(GetOwner()->GetSceneEventBus(), config.effectName, r.hitPos, normal);
}

Math::Vector3 HitReactionComponent::ComputeBodyHitNormal(const HitResolved& r)
{
	// hitNormalは正規化済み。ゼロ(未設定)の場合のみ上向きで代用。
	Math::Vector3 normal = r.hitNormal;
	if (normal.LengthSquared() < 1e-6f) {
		normal = Math::Vector3(0.0f, 1.0f, 0.0f);
	}

	// 法線を攻撃側へ向け、hitNormalの符号規約に依存しないようにする。
	if (r.incomingDir.Dot(normal) > 0.0f) {
		normal = -normal;
	}
	return normal;
}

Math::Vector3 HitReactionComponent::BlendReflectDirection(
	const Math::Vector3& normal, const Math::Vector3& incoming, float normalBlendRatio)
{
	// 反射ベクトル r = d - 2(d·n)n を法線と合成する。
	const Math::Vector3 reflect = incoming - normal * (2.0f * incoming.Dot(normal));
	const float t = std::clamp(normalBlendRatio, 0.0f, 1.0f);

	Math::Vector3 dir = normal * t + reflect * (1.0f - t);
	if (dir.LengthSquared() < 1e-6f) return normal;
	dir.Normalize();
	return dir;
}

std::optional<ShapeSurfaceQuery::ClosestPair> HitReactionComponent::FindWeaponClashPoints(
	const HitResolved& r) const
{
	GameObject* weaponObject = r.weaponObject.Resolve();
	if (weaponObject == nullptr) return std::nullopt;

	WeaponComponent* myWeapon = weapon_.Resolve();
	if (myWeapon == nullptr) return std::nullopt;

	ColliderComponent* myCollider = myWeapon->GetCollider();
	ColliderComponent* attackerCollider = weaponObject->GetComponent<ColliderComponent>();
	if (myCollider == nullptr || attackerCollider == nullptr) return std::nullopt;

	const CollisionShapeEntry* myShape = myCollider->FindShape(kWeaponShapeName);
	const CollisionShapeEntry* attackerShape = attackerCollider->FindShape(kWeaponShapeName);
	if (myShape == nullptr || attackerShape == nullptr) return std::nullopt;

	// 自武器上の、攻撃側武器に最も近い点(onA)と、その対応点(onB)。
	return ShapeSurfaceQuery::ClosestPointsBetweenShapes(*myCollider, *myShape, *attackerCollider, *attackerShape);
}
