#include "EnemyFactory.h"
#include "Application/Factories/PrefabFactory.h"

EnemyFactory::EnemyFactory(const std::unordered_map<std::string, std::string>& definitionPaths)
{
	for (const auto& [id, path] : definitionPaths) {
		PrefabDefinition def;
		if (PrefabFactory::LoadFromFile(path, def)) {
			definitions_[id] = std::move(def);
		}
		else {
			OutputDebugStringA(("EnemyFactory: failed to load " + path + "\n").c_str());
		}
	}
}

GameObject* EnemyFactory::CreateEnemy(ObjectManager& objectManager, const std::string& enemyId, const Math::Vector3& position) const
{
	const auto it = definitions_.find(enemyId);
	if (it == definitions_.end()) return nullptr;
	return PrefabFactory::Create(objectManager, it->second, position);
}

bool EnemyFactory::IsKnownEnemy(const std::string& enemyId) const
{
	return definitions_.find(enemyId) != definitions_.end();
}
