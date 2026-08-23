#pragma once
#include "../ComponentBase.h"
#include "../../Core/Handle.h"

// ============================================================
// AttackSourceComponent
//
// HitBox(ColliderShape側でcategoryMask=HitBox, isTrigger=trueにした
// 形状)を持つGameObjectにアタッチしておく、攻撃1回分のパラメータ置き場。
// CollisionSystemが発行するCollisionEnterEventには「誰が当たったか」
// (GameObject*)しか載っていないため、被弾側(EnemyStatusController等)は
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

	float damage = 10.0f;
	float knockbackPower = 8.0f;
	float hitStunSeconds = 0.4f;

	// この攻撃をガードされた時に、被弾側の体幹(PostureComponent)へ
	// 与えるダメージ量。
	float postureDamage = 20.0f;

	// ガード時のチップダメージ(HP)の割合。damage * chipDamageRatioが
	// 実際にHealthComponentへ適用される(通常被弾のdamageそのものより
	// 大幅に軽減される想定)。
	float chipDamageRatio = 0.1f;

	// この攻撃がジャスト(パリィ)で受けられた時に、攻撃者側の
	// 体幹(PostureComponent)へ与えるダメージ量。通常のpostureDamageより
	// 大きくするのが基本(弾き返された側が大きく怯む、という設計のため)。
	float parryPostureDamage = 40.0f;

	// この攻撃の持ち主(武器なら、武器を装備しているキャラクター本体)
	// への弱参照。パリィ成立時、被弾側から見て「攻撃者本体の
	// PostureComponentを削る」ために必要になる(otherObjectは武器自体の
	// GameObjectであり、キャラクター本体ではないため)。
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
};