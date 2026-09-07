#pragma once
#include "../Collision/ColliderComponent.h"
#include "../Collision/AttackSourceComponent.h"
#include "../Effect/SlashTrailComponent.h"

// ============================================================
// WeaponComponent
// 「攻撃判定を持つ部位」1つ分の制御をカプセル化する汎用コンポーネント。
// 剣のような装備品だけでなく、素手キャラの拳・足・尻尾など、
// ヒットボックス(ColliderComponent)+ダメージ発生源(AttackSourceComponent)
// +任意のTrailPolygonComponentを持つGameObjectであれば何にでも使える
// (Player/Enemyという概念を一切知らない)。
//
// これまでPlayerStatusControllerが直接保持していた
// weaponCollider_/weaponAttackSource_/weaponTrail_ の3Handleと、
// それに対する操作(SetWeaponHitBoxEnabled/SetWeaponTrailEmitting)を
// このコンポーネントへ集約した。武器(または部位)ごとにGameObjectを分け、
// ソケット経由でキャラクターへアタッチする既存の構成はそのまま踏襲する。
// ============================================================
class WeaponComponent : public ComponentBase
{
public:
	explicit WeaponComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override
	{
		collider_ = GetOwner()->GetComponent<ColliderComponent>();
		attackSource_ = GetOwner()->GetComponent<AttackSourceComponent>();
		trail_ = GetOwner()->GetComponent<SlashTrailComponent>(); // 無い部位(素手等)もあるためnullptr許容
	}

	// 攻撃判定発生窓の開閉。有効化のタイミングでalreadyHitをクリアするのは
	// 元のPlayerStatusController::SetWeaponHitBoxEnabled()と同じ理由
	// (1回の判定窓で同じ相手に複数回ヒットしないようにするため)。
	void SetHitBoxEnabled(bool enabled)
	{
		if (collider_ != nullptr) {
			collider_->SetShapeEnabled("HitBox", enabled);
		}
		if (enabled && attackSource_ != nullptr) {
			attackSource_->alreadyHit.clear();
		}
	}

	// 武器の軌跡エフェクトの発生/停止。Trailを持たない部位(拳・足等)では
	// trail_がnullptrのまま何もしない。
	void SetTrailEmitting(bool emitting)
	{
		if (trail_ == nullptr) return;
		if (emitting) trail_->StartEmit();
		else trail_->StopEmit();
	}

	ColliderComponent* GetCollider() const { return collider_; }
	AttackSourceComponent* GetAttackSource() const { return attackSource_; }

	// HitReactionComponent等、既存インターフェースがHandle<ColliderComponent>
	// を要求する箇所との橋渡し用。WeaponComponentとColliderComponentは
	// 同一GameObject上の兄弟なので、オーナーからHandleを組み直せる想定
	// (Handle<T>がGameObject*からの構築+内部でのGetComponent解決に
	//  対応している前提。Handle.hの実装に合わせて要調整)。
	//ColliderComponent GetColliderHandle() const { return Handle<ColliderComponent>(GetOwner()); }

private:
	ColliderComponent* collider_ = nullptr;
	AttackSourceComponent* attackSource_ = nullptr;
	SlashTrailComponent* trail_ = nullptr; // 任意。無い部位も許容する
};
