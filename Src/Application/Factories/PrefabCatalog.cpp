#include "PrefabCatalog.h"
#include "Application/Factories/PrefabFactory.h"

PrefabCatalog::PrefabCatalog(const std::unordered_map<std::string, std::string>& paths)
{
	for (const auto& [id, path] : paths) {
		PrefabDefinition def;
		if (PrefabFactory::LoadFromFile(path, def)) {
			definitions_[id] = std::move(def);
		}
		else {
			OutputDebugStringA(("PrefabCatalog: failed to load " + path + "\n").c_str());
		}
	}
}

bool PrefabCatalog::Contains(const std::string& id) const
{
	return definitions_.find(id) != definitions_.end();
}

GameObject* PrefabCatalog::Spawn(ObjectManager& objectManager, const std::string& id) const
{
	const auto it = definitions_.find(id);
	if (it == definitions_.end()) return nullptr;
	return PrefabFactory::Create(objectManager, it->second);
}

GameObject* PrefabCatalog::Spawn(ObjectManager& objectManager, const std::string& id, const Math::Vector3& position) const
{
	const auto it = definitions_.find(id);
	if (it == definitions_.end()) return nullptr;
	return PrefabFactory::Create(objectManager, it->second, position);
}
