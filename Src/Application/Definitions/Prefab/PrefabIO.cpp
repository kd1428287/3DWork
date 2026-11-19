#include "Application/main.h"

#include "PrefabIO.h"
#include "Application/Definitions/Loaders/DefinitionJson.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PrefabDefinition,
	name, pos, rotation, scale, components, children)

namespace
{
	void Compact(nlohmann::json& node)
	{
		if (node.contains("children"))
		{
			if (node["children"].empty()) { node.erase("children"); }
			else { for (auto& child : node["children"]) { Compact(child); } }
		}

		if (node.contains("components"))
		{
			for (auto& c : node["components"])
			{
				if (c.contains("params") && c["params"].is_object() && c["params"].empty()) { c.erase("params"); }
			}
		}
	}
}

nlohmann::json PrefabToJson(const PrefabDefinition& def)
{
	nlohmann::json j = def;
	Compact(j);
	return j;
}

bool LoadPrefabFile(const std::string& path, PrefabDefinition& out)
{
	nlohmann::json root;
	if (!JsonLoader::Load(path, root)) return false;

	try {
		out = root.get<PrefabDefinition>();
		return true;
	}
	catch (const std::exception& e) {
		OutputDebugStringA(("PrefabFactory: " + path + ": " + e.what() + "\n").c_str());
		return false;
	}
}

bool SavePrefabFile(const std::string& path, const PrefabDefinition& def)
{
	return JsonLoader::Save(path, PrefabToJson(def));
}
