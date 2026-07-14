#pragma once
#include "../GameObjectFactory.h"

class EnemyFactory {
public:
	// カード定義データベースを受け取り、登録できるものを全て登録しておく。
	// database自体は所有しない(この後変更されない前提)。
	explicit EnemyFactory(const std::unordered_map<std::string, EnemyDefinition>& database) {
		for (const auto& [id, def] : database) {
			const EnemyDefinition* defPtr = &def;  // database不変前提で安全に保持できる
			registry_.Register(id, [defPtr](ObjectManager& objectManager, int ownerPlayerId) {
				GameObject* Enemy = objectManager.Instantiate(defPtr->name);
				Enemy->AddComponent<TransformComponent>();
				return Enemy;
				});
		}
	}

	GameObject* CreateEnemy(ObjectManager& objectManager, const std::string& EnemyId) {
		return registry_.Create(EnemyId, objectManager);
	}

	bool IsKnownEnemy(const std::string& EnemyId) const {
		return registry_.IsRegistered(EnemyId);
	}

private:
	GameObjectFactory<int> registry_;
};