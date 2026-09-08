#pragma once
#include "../Collision/ColliderComponent.h"
#include "../Collision/AttackSourceComponent.h"
#include "../Effect/SlashTrailComponent.h"

// ============================================================
// WeaponComponent
// 「攻撃判定を持つ部位」1つ分の制御をカプセル化する汎用コンポーネント。
// 剣のような装備品だけでなく、素手キャラの拳・足・尻尾など、
// ヒットボックス(ColliderComponent)+ダメージ発生源(AttackSourceComponent)
// +任意のSlashTrailComponentを持つGameObjectであれば何にでも使える
// (Player/Enemyという概念を一切知らない)。
//
// これまでPlayerStatusControllerが直接保持していた
// weaponCollider_/weaponAttackSource_/weaponTrail_ の3Handleと、
// それに対する操作(SetWeaponHitBoxEnabled/SetWeaponTrailEmitting)を
// このコンポーネントへ集約した。武器(または部位)ごとにGameObjectを分け、
// ソケット経由でキャラクターへアタッチする既存の構成はそのまま踏襲する。
//
// 【Handleについて】
// このコンポーネント自身はHandleを一切扱わない。Handle<T>はT*を必要とする
// 側(受け取る側/渡す側)がその場で組み立てるものであり、参照元となる
// コンポーネントが「渡す用のHandle」をあらかじめ用意しておく必要は無い
// (Handle.h参照)。他コンポーネントへHandle<ColliderComponent>を渡したい
// 場合は、呼び出し側がGetCollider()の結果からHandle<ColliderComponent>を
// その場で構築する(PlayerStatusController::Start()参照)。
// ============================================================
class WeaponComponent : public ComponentBase
{
public:
	explicit WeaponComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override
	{
		// Start()ではなくAwake()で解決する。武器はキャラクター(Player)とは
		// 別のGameObjectであり、Start()の呼び出し順序はGameObjectをまたぐと
		// 保証されない(PlayerStatusController::Start()の方が先に走る
		// 構成だと、そちらからGetCollider()した時点でまだnullptrになりうる)。
		// AwakeはHitReactionComponentの購読処理と同じく、AddComponent直後に
		// 同期的に実行される想定のフックのため、こちらに寄せる。
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

private:
	ColliderComponent* collider_ = nullptr;
	AttackSourceComponent* attackSource_ = nullptr;
	SlashTrailComponent* trail_ = nullptr; // 任意。無い部位も許容する
};