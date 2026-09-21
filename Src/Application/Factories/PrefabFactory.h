#pragma once

#include "Application/Definitions/Prefab/Prefab.h"

class GameObject;
class ObjectManager;

namespace PrefabFactory
{
	// defからGameObjectを生成する(Transform→components→childrenの順)。
	// 失敗したコンポーネントはログに出して飛ばし、オブジェクト自体は生成する。
	GameObject* Create(ObjectManager& objectManager, const PrefabDefinition& def);

	// 位置だけを差し替えて生成する(敵のスポーン位置など)。
	GameObject* Create(ObjectManager& objectManager, const PrefabDefinition& def, const Math::Vector3& position);

	// 既存のGameObjectへcomponentsだけを追加する。1つでも失敗したらfalseを返す。
	bool AddComponents(ObjectManager& objectManager, GameObject& obj,
		const std::vector<ComponentEntry>& components, GameObject* parent = nullptr);

	// JSONファイルからPrefabDefinitionを読む。失敗時はfalseを返し、outは変更しない。
	bool LoadFromFile(const std::string& path, PrefabDefinition& out);
}
