#pragma once

#include "Application/Definitions/Character/Common/CombatData.h"

// ============================================================
// AttackSourceComponent
//
// HitBox(ColliderShape側でcategoryMask=HitBox, isTrigger=trueにした
// 形状)を持つGameObjectにアタッチしておく、攻撃1回分のパラメータ置き場。
// CollisionSystemが発行するCollisionEnterEventには「誰が当たったか」
// (GameObject*)しか載っていないため、被弾側(EnemyAIController等)は
// e.otherObject->GetComponent<AttackSourceComponent>()で攻撃データを
// 引き当てる想定。
//
// 武器やスキルごとに値を変えたい場合は、GameObject生成時にこの
// コンポーネントの値を書き換えるか、派生クラスを作って上書きする。
//
// HitBoxのenabled切り替え(攻撃発生フレームだけ判定させる)と組み合わせて
// 使う想定。PlayerStatusController::SetWeaponHitBoxEnabled()参照。
// ============================================================
class AttackSourceComponent : public ComponentBase {
public:
	explicit AttackSourceComponent(GameObject* owner) : ComponentBase(owner) {}

	// パリィされた際に、攻撃者自身(ownerCharacter)のローカルEventBusへ
	// 発行されるイベント。「反応するかどうか」「どう反応するか」は
	// 完全に攻撃者側(購読する側)に委ねる。被弾側(PlayerStatusController等)
	// はGameObjectの具体的な型を一切知らずにこれをPublish()するだけで
	// 済む(HealthComponent::DiedEvent/Events::Collision::CollisionEnterEvent
	// と同じ、GameObjectローカルバス経由の疎結合パターン)。
	//
	// 【現状】パリィ時のリアクション自体は未実装(一旦保留)。将来Enemy側に
	// 反応を実装する際は、そのコンポーネントのStart()でこのイベントを
	// Subscribe<AttackSourceComponent::ParriedEvent>()すればよい。
	struct ParriedEvent : public Event
	{
		// パリィが成立した攻撃自身のparryPostureDamageをそのまま運ぶ。
		// 発行側(パリィ判定を行う箇所、Player側のガード/パリィ判定コードを
		// 想定。未実装)がこのAttackSourceComponentインスタンスから読み取って
		// ここへ詰める。攻撃(武器/技)ごとに値を変えたい(parryPostureDamage
		// フィールド自体のコメント参照)ため、値そのものをイベントへ
		// 載せて運ぶ必要がある。
		float parryPostureDamage = 0.0f;
	};

	
	// この攻撃の持ち主(武器なら、武器を装備しているキャラクター本体)
	// への弱参照。パリィ成立時、被弾側から見て「攻撃者本体の
	// PostureComponentを削る」「攻撃者本体へParriedEventを発行する」ために
	// 必要になる(otherObjectは武器自体のGameObjectであり、キャラクター
	// 本体ではないため)。
	// 生成側(PlayerFactory::CreateWeapon等)が明示的にセットすること。
	// 未設定(Resolve()がnullptr)のままでも、その場合はパリィ処理側が
	// 何もしないだけで安全に動作する。
	Handle<GameObject> ownerCharacter;

	// 1回の攻撃で同じ相手に多段ヒットさせたくない場合、ここに
	// 既にヒットしたGameObjectを記録して、CollisionEnterEvent受信側
	// (攻撃側のロジック)で判定に使う。HurtBox側の実装だけでは
	// 多段ヒット防止はできないので、必要ならここを使う。
	// 例: if (alreadyHit.contains(e.otherObject)) return;
	std::unordered_set<GameObject*> alreadyHit;

	void SetAttackDamageData(AttackDamageData data) { damageData_ = data; }
	const AttackDamageData& GetAttackDamageData() const { return damageData_; }

private:
	AttackDamageData damageData_;

};