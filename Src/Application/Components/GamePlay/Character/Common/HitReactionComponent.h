#pragma once
#include "AttackSourceComponent.h"
#include "PostureComponent.h"
#include "HealthComponent.h"
#include "../../../Physics/Movement/VelocityComponent.h"
#include "../../../Tags/IHitReactionQuery.h"
#include "Application/Core/EventBus/Events/CollisionEvents.h"
#include "Application/Definitions/Prefab/BitFlags.h" 

class WeaponComponent;

// ============================================================
// 【HitReactionConfigへの統合について】
// 以前はSetLargeStaggerDuration()という個別セッター1つと、
// kGuardKnockbackPower/カメラシェイク強度/ヒットストップ秒数/被弾
// エフェクト名等のハードコードされた値が混在していた。Enemy側で
// このコンポーネントを使うにあたり、これらをHitReactionConfig
// (Definitions/Character/Common/CharacterDefinitionCommon.h)へ
// 一括化し、SetConfig()1回で注入する形にした。
// デフォルト値は導入前のPlayer側の挙動をそのまま踏襲しているため、
// Player側は何も変更しなくても従来通り動作する。
// ============================================================

enum class HitReactionFlags : uint32_t
{
	None = 0,
	CameraShake = 1u << 0, // 通常被弾時にカメラシェイクを発生させるか
	HitStop = 1u << 1, // 通常被弾時にヒットストップを発生させるか
	WeaponClashFx = 1u << 2, // ガード/パリィ時に鍔迫り合いエフェクトを出すか
};

template <>
struct EnumFlagStringMap<HitReactionFlags> {
	static constexpr std::pair<const char*, HitReactionFlags> entries[] = {
		{ "CameraShake",   HitReactionFlags::CameraShake },
		{ "HitStop",       HitReactionFlags::HitStop },
		{ "WeaponClashFx", HitReactionFlags::WeaponClashFx },
	};
};

struct HitReactionConfig
{
	BitFlags<HitReactionFlags> effectFlags;

	std::string damageEffectName	= "BloodSplatter";
	std::string parryEffectName		= "WeaponClashParry";
	std::string blockEffectName		= "WeaponClashBlock";

	float cameraShakeIntensity		= 0.75f;
	float hitStopDelaySeconds		= 0.0f;
	float hitStopDurationSeconds	= 0.1f;
	float guardKnockbackPower		= 2.0f;
	float largeStaggerDuration		= 0.6f;
};

class HitReactionComponent : public ComponentBase
{
public:
	explicit HitReactionComponent(GameObject* owner) : ComponentBase(owner) {}

	using Config = HitReactionConfig;
	void SetConfig(const Config& config) { config_ = config; }

	void Awake() override;

	// パリィ/ガード/通常被弾のどれで判定するかを問い合わせる相手を登録する
	void SetQuerySource(IHitReactionQuery* query) { query_ = query; }

	// 被弾時、鍔迫り合いの火花エフェクトの発生元として使う自分の武器
	void SetWeapon(Handle<WeaponComponent> weapon) { weapon_ = weapon; }

private:
	void OnCollisionEnter(const Events::Collision::CollisionEnterEvent& e);
	void SpawnWeaponClashEffect(GameObject* attackerWeaponObj, bool isParry);
	void SpawnDamageEffect(GameObject* self, GameObject* attackerWeaponObj);

	Math::Vector3 ComputeKnockbackDirection(GameObject* attacker) const;

	IHitReactionQuery* query_ = nullptr;

	PostureComponent* postureComponent_ = nullptr;
	HealthComponent* healthComponent_ = nullptr;
	VelocityComponent* velocityComponent_ = nullptr;
	TransformComponent* transform_ = nullptr;

	Handle<WeaponComponent> weapon_;

	HitReactionConfig config_;

	ScopedSubscriber subscriber_;
};