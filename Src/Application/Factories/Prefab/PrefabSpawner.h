#pragma once
#include <string>
#include "Application/Factories/Prefab/PrefabCatalog.h"

// プレハブidで生成と破棄を依頼する公開窓口。生成物はHandleで返し、保持は呼び出し側が行う。
class PrefabSpawner
{
public:
	PrefabSpawner(const PrefabCatalog& catalog, ObjectManager& objectManager)
		: catalog_(catalog), objectManager_(objectManager) {}

	bool Contains(const std::string& id) const { return catalog_.Contains(id); }

	// 未登録のidは無効なHandleを返す。positionを省略するとPrefabのposを使う。
	Handle<GameObject> Spawn(const std::string& id);
	Handle<GameObject> Spawn(const std::string& id, const Math::Vector3& position);

	// 破棄。IDespawnHandlerを持つ型はその後始末に任せ、無ければ即Destroyする。
	void Despawn(Handle<GameObject> obj);

private:
	const PrefabCatalog& catalog_;
	ObjectManager& objectManager_;
};
