#include "PrefabCatalog.h"
#include "Application/Definitions/Prefab/PrefabIO.h"

PrefabCatalog::PrefabCatalog(const std::unordered_map<std::string, std::string>& paths)
{
	for (const auto& [id, path] : paths) {
		PrefabDefinition def;
		if (LoadPrefabFile(path, def)) {
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

const PrefabDefinition* PrefabCatalog::Find(const std::string& id) const
{
	const auto it = definitions_.find(id);
	return it != definitions_.end() ? &it->second : nullptr;
}
