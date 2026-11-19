#include "Application/main.h"

#include "CharacterData.h"
#include "Application/Definitions/Prefab/PrefabIO.h"

namespace
{
	// Prefab→ノード。IDは深さ優先の出現順で採番する(同じ構造なら再読込でも同じIDになる)
	void FromPrefab(const PrefabDefinition& def, CharacterNode& node, NodeId& nextId)
	{
		node.id = nextId++;
		node.name = def.name;
		node.pos = def.pos;
		node.rotation = def.rotation;
		node.scale = def.scale;
		node.components = def.components;

		node.children.reserve(def.children.size());
		for (const PrefabDefinition& c : def.children)
		{
			CharacterNode child;
			FromPrefab(c, child, nextId);
			node.children.push_back(std::move(child));
		}
	}

	PrefabDefinition ToPrefab(const CharacterNode& node)
	{
		PrefabDefinition def;
		def.name = node.name;
		def.pos = node.pos;
		def.rotation = node.rotation;
		def.scale = node.scale;
		def.components = node.components;

		def.children.reserve(node.children.size());
		for (const CharacterNode& c : node.children) { def.children.push_back(ToPrefab(c)); }
		return def;
	}
}

bool LoadCharacterFile(const std::string& path, CharacterFile& out)
{
	PrefabDefinition def;
	if (!LoadPrefabFile(path, def)) return false;

	CharacterFile f;
	FromPrefab(def, f.root, f.nextId);
	out = std::move(f);
	return true;
}

bool SaveCharacterFile(const std::string& path, const CharacterFile& file)
{
	return SavePrefabFile(path, ToPrefab(file.root));
}

nlohmann::json CharacterFileToJson(const CharacterFile& file)
{
	return PrefabToJson(ToPrefab(file.root));
}

CharacterNode* FindNodeById(CharacterNode& root, NodeId id)
{
	if (root.id == id) return &root;
	for (auto& c : root.children)
	{
		if (CharacterNode* r = FindNodeById(c, id)) return r;
	}
	return nullptr;
}

CharacterNode* FindParentOf(CharacterNode& root, NodeId id)
{
	for (auto& c : root.children)
	{
		if (c.id == id) return &root;
		if (CharacterNode* r = FindParentOf(c, id)) return r;
	}
	return nullptr;
}

void ReassignNodeIds(CharacterNode& node, NodeId& nextId)
{
	node.id = nextId++;
	for (auto& c : node.children) { ReassignNodeIds(c, nextId); }
}
