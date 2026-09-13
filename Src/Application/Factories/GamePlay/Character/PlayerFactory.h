#pragma once

#include "Application/Definitions/Character/Player/PlayerDefinition.h"

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

	GameObject* CreateWeapon(ObjectManager& objectManager, GameObject* player, Handle<TransformComponent>& handle, const WeaponDefinition& weaponDefinition, const IKChainDefinition& ikChain);
};
