#pragma once
#include <string>

class ObjectManager;
class TerrainFactory;

class MapLoader {
public:
	/**
	 * @brief JSONファイルからマップを読み込み、オブジェクトを生成する
	 * @param jsonFilePath 読み込むファイルのパス
	 * @param objectManager オブジェクト管理システム
	 * @param terrainFactory 構築を委譲するファクトリー
	 * @return 読み込み成功可否
	 */
	bool LoadMapFromJson(const std::string& jsonFilePath,
		ObjectManager& objectManager,
		TerrainFactory& terrainFactory);
};