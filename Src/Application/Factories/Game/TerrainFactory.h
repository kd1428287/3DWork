#pragma once
#include "../GameObjectFactory.h"

class GameObject;

class TerrainFactory {
public:
	// 複数生成させる必要が出てきたときに実装する
	//// database自体は所有しない(この後変更されない前提)。
	//explicit TerrainFactory(const std::unordered_map<std::string, TerrainDefinition>& database) {
	//	for (const auto& [id, def] : database) {
	//		const TerrainDefinition* defPtr = &def;  // database不変前提で安全に保持できる
	//		registry_.Register(id, [defPtr](ObjectManager& objectManager, int ownerPlayerId) {
	//			GameObject* Terrain = objectManager.Instantiate(defPtr->name);
	//			Terrain->AddComponent<TransformComponent>();
	//			return Terrain;
	//			});
	//	}
	//}

	//GameObject* CreateTerrain(ObjectManager& objectManager, const std::string& terrainID) {
	//	return registry_.Create(terrainID, objectManager);
	//}

	//bool IsKnownTerrain(const std::string& terrainID) const {
	//	return registry_.IsRegistered(terrainID);
	//}

	TerrainFactory() = default;
	~TerrainFactory() = default;

	// コピー・ムーブ禁止
	TerrainFactory(const TerrainFactory&) = delete;
	TerrainFactory& operator=(const TerrainFactory&) = delete;

	/**
	 * @brief プレイヤーキャラクターを生成し、必要なコンポーネントをアタッチして返す
	 * @param objectManager オブジェクト管理システム
	 * @param ownerTerrainId マルチプレイ等を見据えたプレイヤー識別ID（不要なら削除可）
	 * @return 生成されたGameObjectのスマートポインタ
	 */
	GameObject* CreateTerrain(ObjectManager& objectManager, int ownerTerrainId = 0);

private:
	GameObjectFactory<int> registry_;
};