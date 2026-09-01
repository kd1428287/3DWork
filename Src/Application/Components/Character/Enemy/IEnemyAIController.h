#pragma once
#include "../../../Core/Handle.h"

class ColliderComponent;
class AttackSourceComponent;
class GameObject;

// ============================================================
// EnemyFactoryが敵種によらず武器の取り付け等を行うための最小限の
// 共通インターフェース。
//
// 【背景】EnemyAIControllerとWarrockAIControllerは意図的に継承関係を
// 持たない完全に独立した実装(WarrockAIController.h冒頭コメント参照)。
// そのためEnemyFactory側で「AI種類によらず武器を取り付ける」処理を
// 書くには、両者が共通で実装する薄いインターフェースが必要になる。
// このインターフェースは「ファクトリーが必要とする最小限の操作」だけを
// 定義し、意思決定ロジック(BT)自体の共通化は意図していない。
// ============================================================
class IEnemyAIController
{
public:
	virtual ~IEnemyAIController() = default;

	virtual void SetWeapon(Handle<ColliderComponent> weaponCollider, Handle<AttackSourceComponent> weaponAttackSource) = 0;

	// 死亡時に道連れで破棄すべき付随オブジェクト(武器・武器ソケット等)を
	// 登録する。死亡処理をまだ持たない敵種(Warrock等)はこのデフォルト
	// 実装(何もしない)のままでよい。死亡処理を実装した時点で、その
	// 敵種のControllerでオーバーライドすること。
	virtual void RegisterOwnedObject(Handle<GameObject> /*obj*/) {}
};