#pragma once

class ObjectManager;
class TerrainFactory;

class MapLoader
{
public:
	/**
	 * @brief JSONファイル(MapEditorが保存した形式)からマップを読み込み、オブジェクトを生成する
	 * @param jsonFilePath 読み込むファイルのパス(MapEditorのSave先と同じスキーマ・
	 *        MapData.h の LoadMapFile() 経由で読む。旧バージョンの形式も自動変換される)
	 * @param objectManager オブジェクト管理システム
	 * @param terrainFactory 構築を委譲するファクトリー
	 * @return 読み込み成功可否(ファイルが無い/JSONとして壊れている場合のみfalse。
	 *         個々のエンティティのフィールド欠落は該当エンティティをスキップするのみで、
	 *         全体の失敗としては扱わない)
	 */
	bool LoadMapFromJson(const std::string& jsonFilePath,
		ObjectManager& objectManager,
		TerrainFactory& terrainFactory);
};