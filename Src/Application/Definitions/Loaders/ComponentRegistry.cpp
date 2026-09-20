#include "ComponentRegistry.h"

#include <stdexcept>

ComponentRegistry& ComponentRegistry::Get()
{
	static ComponentRegistry registry = [] {
		ComponentRegistry r;
		RegisterAllComponents(r);
		return r;
	}();
	return registry;
}

bool ComponentRegistry::AddComponents(GameObject* obj, const nlohmann::json& components) const
{
	if (components.is_null()) return true;
	if (!components.is_array()) {
		OutputDebugStringA("ComponentRegistry: components must be an array\n");
		return false;
	}

	bool ok = true;
	for (const auto& entry : components) {
		try {
			const std::string type = entry.at("type").get<std::string>();
			const auto it = factories_.find(type);
			if (it == factories_.end()) throw std::runtime_error("unknown component type: " + type);
			it->second(obj, entry);
		}
		catch (const std::exception& e) {
			OutputDebugStringA((std::string("ComponentRegistry: ") + e.what() + "\n").c_str());
			ok = false;
		}
	}
	return ok;
}
