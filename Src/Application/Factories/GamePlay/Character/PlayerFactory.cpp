#include "PlayerFactory.h"
#include "Application/Definitions/Prefab/Prefab.h"
#include "../../PrefabFactory.h"

GameObject* PlayerFactory::CreatePlayer(ObjectManager& objectManager, const std::string& definitionPath)
{
	PrefabDefinition def;
	if (!PrefabFactory::LoadFromFile(definitionPath, def)) return nullptr;
	return PrefabFactory::Create(objectManager, def);
}
