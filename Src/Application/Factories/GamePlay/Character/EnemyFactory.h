#pragma once

#include "Application/Factories/PrefabCatalog.h"

// 旧EnemyFactoryの呼び出し側を残すための互換窓口。呼び出し側をPrefabCatalogへ移したら削除する。
class EnemyFactory : public PrefabCatalog {
public:
	using PrefabCatalog::PrefabCatalog;

	GameObject* CreateEnemy(ObjectManager& objectManager, const std::string& enemyId, const Math::Vector3& position) const
	{
		return Spawn(objectManager, enemyId, position);
	}

	bool IsKnownEnemy(const std::string& enemyId) const { return Contains(enemyId); }
};
