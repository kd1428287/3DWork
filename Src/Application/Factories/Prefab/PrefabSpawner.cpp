#include "PrefabSpawner.h"
#include "Application/Factories/Prefab/PrefabFactory.h"
#include "Application/Components/Tags/IDespawnHandler.h"

namespace
{
	// 未登録idはログだけ出して無効なHandleを返す
	Handle<GameObject> UnknownId(const std::string& id)
	{
		OutputDebugStringA(("PrefabSpawner: unknown prefab id " + id + "\n").c_str());
		return Handle<GameObject>{};
	}
}

Handle<GameObject> PrefabSpawner::Spawn(const std::string& id)
{
	const PrefabDefinition* def = catalog_.Find(id);
	if (!def) return UnknownId(id);
	return Handle<GameObject>(PrefabFactory::Create(objectManager_, *def));
}

Handle<GameObject> PrefabSpawner::Spawn(const std::string& id, const Math::Vector3& position)
{
	const PrefabDefinition* def = catalog_.Find(id);
	if (!def) return UnknownId(id);
	return Handle<GameObject>(PrefabFactory::Create(objectManager_, *def, position));
}

void PrefabSpawner::Despawn(Handle<GameObject> obj)
{
	GameObject* target = obj.Resolve();
	if (!target) return;

	// 専用の後始末を持つ型(敵など)はそちらに任せ、無ければそのまま破棄予約する
	const auto handlers = target->GetTagged<IDespawnHandler>();
	if (!handlers.empty()) {
		handlers.front()->RequestDespawn();
		return;
	}
	objectManager_.Destroy(target);
}
