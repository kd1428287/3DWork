#pragma once
#include <string>
#include "Application/Factories/PrefabCatalog.h"

// 敵の生成と削除の窓口。生成した敵の参照は呼び出し側が持つ。
class EnemySpawner
{
public:
	EnemySpawner(const PrefabCatalog& catalog, ObjectManager& objectManager)
		: catalog_(catalog), objectManager_(objectManager) {}

	// 未登録のidならnullptrを返す
	GameObject* Spawn(const std::string& enemyId, const Math::Vector3& position);
	void Despawn(GameObject* enemy);

private:
	const PrefabCatalog& catalog_;
	ObjectManager& objectManager_;
};
