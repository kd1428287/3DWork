#pragma once
#include "AttackSourceComponent.h"
#include "PostureComponent.h"
#include "HealthComponent.h"
#include "../../../Physics/Movement/VelocityComponent.h"
#include "../../../Tags/IHitReactionQuery.h"
#include "Application/Core/EventBus/Events/CollisionEvents.h"
#include "Application/Definitions/Character/Common/CharacterConfigs.h"

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
class HitReactionComponent : public ComponentBase
{
public:
	explicit HitReactionComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override;

	// パリィ/ガード/通常被弾のどれで判定するかを問い合わせる相手を登録する
	void SetQuerySource(IHitReactionQuery* query) { query_ = query; }

	// 被弾時、鍔迫り合いの火花エフェクトの発生元として使う自分の武器
	void SetWeapon(Handle<WeaponComponent> weapon) { weapon_ = weapon; }

	// 副作用まわりのチューニング値を一括で設定する(クラス冒頭コメント参照)。
	void SetConfig(const HitReactionConfig& config) { config_ = config; }

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