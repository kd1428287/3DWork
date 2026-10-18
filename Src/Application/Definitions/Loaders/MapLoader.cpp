#include "MapLoader.h"
#include "Application/Factories/GamePlay/Map/TerrainFactory.h"
#include "../Map/MapData.h"

namespace
{
	// SpawnMarkerを持つエンティティならoutへ追加してtrue。paramsが不正ならroleがNoneのまま回収する
	bool TryExtractSpawnMarker(const MapEntity& entity, std::vector<SpawnMarkerDef>& out)
	{
		for (const auto& c : entity.components)
		{
			if (c.type != "SpawnMarker") continue;

			SpawnMarkerParams p;
			try { p = c.params.get<SpawnMarkerParams>(); }
			catch (const nlohmann::json::exception&) {}

			out.push_back({ entity.id, p.role, p.prefab, entity.pos, entity.rotation });
			return true;
		}
		return false;
	}
}

bool MapLoader::LoadMapFromJson(const std::string& jsonFilePath, ObjectManager& objectManager,
	TerrainFactory& terrainFactory, std::vector<SpawnMarkerDef>* outMarkers)
{
	MapFile mapFile;
	if (!LoadMapFile(jsonFilePath, mapFile))
	{
		std::cerr << "[MapLoader Error] Failed to load map JSON: " << jsonFilePath << std::endl;
		return false;
	}

	std::vector<SpawnMarkerDef> markers;
	for (const auto& entity : mapFile.entities)
	{
		if (TryExtractSpawnMarker(entity, markers)) continue;
		terrainFactory.CreateFromData(objectManager, entity);
	}

	std::cout << "[MapLoader] Map loaded: " << jsonFilePath
		<< " (entities: " << mapFile.entities.size()
		<< ", spawn markers: " << markers.size() << ")" << std::endl;

	if (outMarkers) *outMarkers = std::move(markers);

	return true;
}
