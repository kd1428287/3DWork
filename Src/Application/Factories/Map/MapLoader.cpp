#include "MapLoader.h"
#include "TerrainFactory.h"
#include "MapData.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

bool MapLoader::LoadMapFromJson(const std::string& jsonFilePath, ObjectManager& objectManager, TerrainFactory& terrainFactory)
{
	std::ifstream file(jsonFilePath);
	if (!file.is_open()) {
		std::cerr << "[MapLoader Error] Failed to open map JSON: " << jsonFilePath << std::endl;
		return false;
	}

	json mapJson;
	try {
		file >> mapJson;
	}
	catch (const json::parse_error& e) {
		std::cerr << "[MapLoader Error] JSON parse error: " << e.what() << std::endl;
		return false;
	}

	// エンティティ配列のパースと生成
	if (mapJson.contains("entities") && mapJson["entities"].is_array()) {
		for (const auto& entityJson : mapJson["entities"]) {
			EntityData data;
			data.id = entityJson.value("id", "unknown");
			data.type = entityJson.value("type", "");
			data.assetId = entityJson.value("asset_id", "");

			if (entityJson.contains("transform")) {
				const auto& tf = entityJson["transform"];
				if (tf.contains("position")) {
					data.transform.position = { tf["position"][0], tf["position"][1], tf["position"][2] };
				}
				if (tf.contains("rotation")) {
					data.transform.rotation = { tf["rotation"][0], tf["rotation"][1], tf["rotation"][2] };
				}
				if (tf.contains("scale")) {
					data.transform.scale = { tf["scale"][0], tf["scale"][1], tf["scale"][2] };
				}
			}

			if (entityJson.contains("properties")) {
				data.colliderType = entityJson["properties"].value("collider_type", "Box");
			}

			// 解析したデータをFactoryに投げて生成させる
			terrainFactory.CreateFromData(objectManager, data);
		}
	}

	std::cout << "[MapLoader] Map loaded successfully: " << jsonFilePath << std::endl;
	return true;
}