#pragma once
#include "../ComponentBase.h"

// ============================================================
// AttackHitBoxComponent
//
// HitBox(ColliderShape側でlayer=HitBox, isTrigger=trueにした形状)を
// 持つGameObjectにアタッチしておく、攻撃1回分のパラメータ置き場。
// CollisionSystemが発行するCollisionEnterEventには「誰が当たったか」
// (GameObject*)しか載っていないため、被弾側(EnemyStatusController等)は
// e.otherObject->GetComponent<AttackHitBoxComponent>()で攻撃データを
// 引き当てる想定。
//
// 武器やスキルごとに値を変えたい場合は、GameObject生成時にこの
// コンポーネントの値を書き換えるか、派生クラスを作って上書きする。
// ============================================================
class AttackSourceComponent : public ComponentBase {
public:
	explicit AttackSourceComponent(GameObject* owner) : ComponentBase(owner) {}

	float damage = 10.0f;
	float knockbackPower = 8.0f;
	float hitStunSeconds = 0.4f;

	// 1回の攻撃で同じ相手に多段ヒットさせたくない場合、ここに
	// 既にヒットしたGameObjectを記録して、CollisionEnterEvent受信側
	// (攻撃側のロジック)で判定に使う。HurtBox側の実装だけでは
	// 多段ヒット防止はできないので、必要ならここを使う。
	// 例: if (alreadyHit.contains(e.otherObject)) return;
	std::unordered_set<GameObject*> alreadyHit;
};
