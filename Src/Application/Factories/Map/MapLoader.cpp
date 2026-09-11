#include "MapLoader.h"
#include "TerrainFactory.h"
#include "MapData.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// JSON側の1エンティティ分を EntityData に変換する
//	フィールド欠落・型不一致はここで個別に検知し、そのエンティティだけスキップする
//	(1個の不正データのせいでマップ全体のロードが失敗することを避ける為)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static bool ParseEntity(const json& e, EntityData& out)
{
	try
	{
		// MapEditor(MapObject)の保存キーと1:1対応
		out.name = e.value("name", std::string("Object"));

		if (e.contains("pos"))
		{
			out.transform.position = { e["pos"][0], e["pos"][1], e["pos"][2] };
		}
		if (e.contains("rotate"))
		{
			out.transform.rotation = { e["rotate"][0], e["rotate"][1], e["rotate"][2] };
		}
		if (e.contains("scale"))
		{
			out.transform.scale = { e["scale"][0], e["scale"][1], e["scale"][2] };
		}

		out.components.clear();

		if (e.contains("components") && e["components"].is_array())
		{
			for (const auto& c : e["components"])
			{
				ComponentEntry entry;
				entry.type = c.value("type", std::string());
				if (entry.type.empty()) continue;	// typeの無いエントリは無視

				entry.params = c.value("params", nlohmann::json::object());
				out.components.push_back(std::move(entry));
			}
		}
	}
	catch (const json::exception& ex)
	{
		std::cerr << "[MapLoader Error] Failed to parse entity: " << ex.what() << std::endl;
		return false;
	}

	return true;
}

bool MapLoader::LoadMapFromJson(const std::string& jsonFilePath, ObjectManager& objectManager, TerrainFactory& terrainFactory)
{
	std::ifstream file(jsonFilePath);
	if (!file.is_open())
	{
		std::cerr << "[MapLoader Error] Failed to open map JSON: " << jsonFilePath << std::endl;
		return false;
	}

	json mapJson;
	try
	{
		file >> mapJson;
	}
	catch (const json::parse_error& e)
	{
		std::cerr << "[MapLoader Error] JSON parse error: " << e.what() << std::endl;
		return false;
	}

	if (!mapJson.is_array())
	{
		std::cerr << "[MapLoader Error] Map JSON root is not an array: " << jsonFilePath << std::endl;
		return false;
	}

	int successCount = 0;
	int skipCount = 0;

	for (const auto& entityJson : mapJson)
	{
		EntityData data;
		if (!ParseEntity(entityJson, data))
		{
			++skipCount;
			continue;
		}

		// 解析したデータをFactoryに投げて生成させる
		terrainFactory.CreateFromData(objectManager, data);
		++successCount;
	}

	std::cout << "[MapLoader] Map loaded: " << jsonFilePath
		<< " (entities: " << successCount << ", skipped: " << skipCount << ")" << std::endl;

	return true;
}