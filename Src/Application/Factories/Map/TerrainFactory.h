#pragma once
#include "../GameObjectFactory.h"
#include "MapData.h" // 構造体をインクルード

class GameObject;

class TerrainFactory {
public:
	TerrainFactory() = default;
	~TerrainFactory() = default;
	TerrainFactory(const TerrainFactory&) = delete;
	TerrainFactory& operator=(const TerrainFactory&) = delete;

	// 従来の固定マップ生成用（不要であれば削除可）
	GameObject* CreateTerrain(ObjectManager& objectManager, int ownerTerrainId = 0);

	/**
	 * @brief 渡されたエンティティデータに基づいてオブジェクトを生成・配置する
	 */
	GameObject* CreateFromData(ObjectManager& objectManager, const EntityData& data);

private:
	GameObjectFactory<int> registry_;
};