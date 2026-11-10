#include "Application/main.h"

#include "CharacterData.h"

namespace
{
	// JSON→ノード。IDは深さ優先の出現順で採番する(同じ構造なら再読込でも同じIDになる)
	void ParseNode(const nlohmann::json& j, CharacterNode& node, NodeId& nextId)
	{
		node.id = nextId++;
		node.name = j.value("name", std::string());
		node.extra = nlohmann::json::object();

		for (auto it = j.begin(); it != j.end(); ++it)
		{
			const std::string& key = it.key();
			if (key == "name" || key == "components" || key == "children") continue;
			node.extra[key] = it.value();
		}

		if (j.contains("components") && j["components"].is_array())
		{
			for (const auto& c : j["components"])
			{
				ComponentEntry e;
				e.type = c.value("type", std::string());
				e.params = c.contains("params") ? c["params"] : nlohmann::json::object();
				node.components.push_back(std::move(e));
			}
		}

		if (j.contains("children") && j["children"].is_array())
		{
			for (const auto& c : j["children"])
			{
				CharacterNode child;
				ParseNode(c, child, nextId);
				node.children.push_back(std::move(child));
			}
		}
	}

	nlohmann::json NodeToJson(const CharacterNode& node)
	{
		nlohmann::json j = node.extra.is_object() ? node.extra : nlohmann::json::object();
		j["name"] = node.name;

		nlohmann::json comps = nlohmann::json::array();
		for (const auto& c : node.components)
		{
			nlohmann::json e;
			e["type"] = c.type;
			// paramsが空のコンポーネントは {"type":"X"} だけにして差分を小さく保つ
			if (!c.params.is_null() && !(c.params.is_object() && c.params.empty()))
			{
				e["params"] = c.params;
			}
			comps.push_back(std::move(e));
		}
		j["components"] = std::move(comps);

		if (!node.children.empty())
		{
			nlohmann::json kids = nlohmann::json::array();
			for (const auto& c : node.children) { kids.push_back(NodeToJson(c)); }
			j["children"] = std::move(kids);
		}
		return j;
	}
}

bool LoadCharacterFile(const std::string& path, CharacterFile& out)
{
	nlohmann::json j;
	if (!JsonLoader::Load(path, j) || !j.is_object()) return false;

	try
	{
		CharacterFile f;
		ParseNode(j, f.root, f.nextId);
		out = std::move(f);
		return true;
	}
	catch (const nlohmann::json::exception&)
	{
		return false;
	}
}

bool SaveCharacterFile(const std::string& path, const CharacterFile& file)
{
	return JsonLoader::Save(path, NodeToJson(file.root));
}

void ReassignNodeIds(CharacterNode& node, NodeId& nextId)
{
	node.id = nextId++;
	for (auto& c : node.children) { ReassignNodeIds(c, nextId); }
}
