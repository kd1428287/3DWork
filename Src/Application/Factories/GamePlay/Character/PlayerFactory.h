#pragma once

class GameObject;
class ObjectManager;

// Player生成の窓口。中身はPrefab(JSON)からの生成で、呼び出し側がPrefabFactoryへ移行するまでの橋渡し。
class PlayerFactory {
public:
	PlayerFactory() = default;
	~PlayerFactory() = default;

	// コピー・ムーブ禁止
	PlayerFactory(const PlayerFactory&) = delete;
	PlayerFactory& operator=(const PlayerFactory&) = delete;

	// definitionPathのPrefabからプレイヤーを生成する。読み込みに失敗した場合はnullptr。
	GameObject* CreatePlayer(ObjectManager& objectManager, const std::string& definitionPath);
};
