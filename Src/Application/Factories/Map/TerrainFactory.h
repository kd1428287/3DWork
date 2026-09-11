#pragma once
#include "../GameObjectFactory.h"
#include "MapData.h" // 構造体をインクルード

class GameObject;

class TerrainFactory
{
public:
	TerrainFactory();
	~TerrainFactory() = default;
	TerrainFactory(const TerrainFactory&) = delete;
	TerrainFactory& operator=(const TerrainFactory&) = delete;

	// 従来の固定マップ生成用（不要であれば削除可）
	GameObject* CreateTerrain(ObjectManager& objectManager, int ownerTerrainId = 0);
	GameObject* CreateSkydome(ObjectManager& objectManager, int ownerTerrainId = 0);

	/**
	 * @brief 渡されたエンティティデータ(MapEditorが保存したマップ由来)に基づいて
	 *        オブジェクトを生成・配置する。
	 *        data.components に列挙された種類をComponentRegistry経由で1つずつ実体化する。
	 *        登録されていない種類はログを出してスキップする(オブジェクト自体は生成する)
	 */
	GameObject* CreateFromData(ObjectManager& objectManager, const EntityData& data);

private:
	GameObjectFactory<int> registry_;
};