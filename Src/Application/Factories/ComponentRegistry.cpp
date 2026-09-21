#include "ComponentRegistry.h"

#include <algorithm>

ComponentRegistry& ComponentRegistry::Instance()
{
	static ComponentRegistry registry = [] {
		ComponentRegistry r;
		RegisterAllComponents(r);
		return r;
		}();
	return registry;
}

void ComponentRegistry::Build(BuildContext& ctx, const ComponentEntry& entry) const
{
	const auto it = types_.find(entry.type);
	if (it == types_.end()) throw std::runtime_error("unknown component type: " + entry.type);
	it->second.factory(ctx, entry.params);
}

std::vector<std::string> ComponentRegistry::GetTypeNames() const
{
	std::vector<std::string> names;
	names.reserve(types_.size());
	for (const auto& kv : types_) names.push_back(kv.first);
	std::sort(names.begin(), names.end());
	return names;
}

const nlohmann::json* ComponentRegistry::FindDefaultParams(const std::string& type) const
{
	const auto it = types_.find(type);
	return it != types_.end() ? &it->second.defaultParams : nullptr;
}