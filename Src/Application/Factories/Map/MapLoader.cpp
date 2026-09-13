#include "MapLoader.h"
#include "TerrainFactory.h"
#include "MapData.h"

#include <iostream>

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