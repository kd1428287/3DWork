#include "PrefabFactory.h"
#include "ComponentRegistry.h"

#include "Application/Definitions/Loaders/DefinitionJson.h"
#include "Application/Components/Core/TransformComponent.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PrefabDefinition,
	name, pos, rotation, scale, components, children)

namespace
{
	GameObject* CreateImpl(ObjectManager& objectManager, const PrefabDefinition& def,
		const Math::Vector3& position, GameObject* parent)
	{
		GameObject* obj = objectManager.Instantiate(def.name);
		if (!obj) return nullptr;

		TransformComponent* transform = obj->AddComponent<TransformComponent>();
		transform->SetPosition(position);
		transform->SetRotation(def.rotation);
		transform->SetScale(def.scale);

		PrefabFactory::AddComponents(objectManager, *obj, def.components, parent);

		for (const PrefabDefinition& child : def.children) {
			CreateImpl(objectManager, child, child.pos, obj);
		}
		return obj;
	}
}

GameObject* PrefabFactory::Create(ObjectManager& objectManager, const PrefabDefinition& def)
{
	return CreateImpl(objectManager, def, def.pos, nullptr);
}

GameObject* PrefabFactory::Create(ObjectManager& objectManager, const PrefabDefinition& def, const Math::Vector3& position)
{
	return CreateImpl(objectManager, def, position, nullptr);
}

bool PrefabFactory::AddComponents(ObjectManager& objectManager, GameObject& obj,
	const std::vector<ComponentEntry>& components, GameObject* parent)
{
	BuildContext ctx{ objectManager, obj, parent };
	bool ok = true;

	for (const ComponentEntry& entry : components) {
		try {
			ComponentRegistry::Instance().Build(ctx, entry);
		}
		catch (const std::exception& e) {
			OutputDebugStringA(("PrefabFactory: " + obj.GetName() + " / " + entry.type + ": " + e.what() + "\n").c_str());
			ok = false;
		}
	}
	return ok;
}

bool PrefabFactory::LoadFromFile(const std::string& path, PrefabDefinition& out)
{
	nlohmann::json root;
	if (!JsonLoader::Load(path, root)) return false;

	try {
		out = root.get<PrefabDefinition>();
		return true;
	}
	catch (const std::exception& e) {
		OutputDebugStringA(("PrefabFactory: " + path + ": " + e.what() + "\n").c_str());
		return false;
	}
}
