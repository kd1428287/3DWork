#include "MapLoader.h"
#include "Application/Factories/GamePlay/Map/TerrainFactory.h"
#include "../Map/MapData.h"

bool MapLoader::LoadMapFromJson(const std::string& jsonFilePath, ObjectManager& objectManager, TerrainFactory& terrainFactory)
{
	MapFile mapFile;
	if (!LoadMapFile(jsonFilePath, mapFile))
	{
		std::cerr << "[MapLoader Error] Failed to load map JSON: " << jsonFilePath << std::endl;
		return false;
	}

	for (const auto& entity : mapFile.entities)
	{
		terrainFactory.CreateFromData(objectManager, entity);
	}

	std::cout << "[MapLoader] Map loaded: " << jsonFilePath
		<< " (entities: " << mapFile.entities.size() << ")" << std::endl;

	return true;
}