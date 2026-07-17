#pragma once
#include <memory>
#include <string>

// 前方宣言でコンパイル時間を短縮
class GameObject;
class ObjectManager;

// ============================================================
// プレイヤー（自機）の生成に特化したファクトリークラス。
// 無駄なレジストリを排除し、明快な単一責任を持つ。
// ============================================================
class PlayerFactory {
public:
	PlayerFactory() = default;
	~PlayerFactory() = default;

	// コピー・ムーブ禁止
	PlayerFactory(const PlayerFactory&) = delete;
	PlayerFactory& operator=(const PlayerFactory&) = delete;

	GameObject* CreatePlayer(ObjectManager& objectManager);
	GameObject* CreateSocket(ObjectManager& objectManager, std::string objectID ,Handle<TransformComponent>& handle);
	GameObject* CreateWeapon(ObjectManager& objectManager, Handle<TransformComponent>& handle);
};