#pragma once

#include <string>

#include "PlayerDefinition.h"

class GameObject;
class ObjectManager;
class SkeletonComponent;
class BoneSocketComponent;

class PlayerFactory {
public:
	PlayerFactory() = default;
	~PlayerFactory() = default;

	// コピー・ムーブ禁止
	PlayerFactory(const PlayerFactory&) = delete;
	PlayerFactory& operator=(const PlayerFactory&) = delete;

	GameObject* CreatePlayer(ObjectManager& objectManager, const std::string& definitionPath);
	GameObject* CreatePlayer(ObjectManager& objectManager, const PlayerDefinition& definition);

	GameObject* CreateSocket(ObjectManager& objectManager, std::string objectID, Handle<SkeletonComponent>& handle);
	GameObject* CreateWeapon(ObjectManager& objectManager, GameObject* player, Handle<TransformComponent>& handle, const WeaponDefinition& weaponDefinition, const IKChainDefinition& ikChain);
};
