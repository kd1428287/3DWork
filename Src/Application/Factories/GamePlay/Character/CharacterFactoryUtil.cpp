#include "CharacterFactoryUtil.h"
#include "Application/Components/Core/BoneSocketComponent.h"

GameObject* CharacterFactoryUtil::CreateSocket(ObjectManager& objectManager, const std::string& objID, Handle<SkeletonComponent>& handle)
{
	auto* obj = objectManager.Instantiate(objID);
	if (!obj) return nullptr;
	obj->AddComponent<BoneSocketComponent>(handle, objID);
	return obj;
}
