#include "ComponentRegistry.h"

#include <algorithm>
#include <cmath>

#include "Application/Components/Core/TransformComponent.h"

ComponentRegistry& ComponentRegistry::Instance()
{
	static ComponentRegistry registry = [] {
		ComponentRegistry r;
		RegisterAllComponents(r);
		return r;
		}();
	return registry;
}

Math::Vector3 BuildContext::OwnerScale() const
{
	const TransformComponent* transform = self.GetComponent<TransformComponent>();
	if (transform == nullptr) return Math::Vector3::One;

	const Math::Vector3& s = transform->GetScale();
	return { std::abs(s.x), std::abs(s.y), std::abs(s.z) };
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

const nlohmann::ordered_json* ComponentRegistry::FindDefaultParams(const std::string& type) const
{
	const auto it = types_.find(type);
	return it != types_.end() ? &it->second.defaultParams : nullptr;
}