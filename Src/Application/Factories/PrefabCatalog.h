#pragma once

#include "Application/Definitions/Prefab/Prefab.h"

class GameObject;
class ObjectManager;

// id → Prefab定義の対応表。敵のように種類が複数あるものを、idだけで生成するための窓口。
class PrefabCatalog {
public:
	// pathsは id → Prefab JSONファイルパス の対応表。読み込みに失敗した定義はログに出して飛ばす。
	explicit PrefabCatalog(const std::unordered_map<std::string, std::string>& paths);
	~PrefabCatalog() = default;

	PrefabCatalog(const PrefabCatalog&) = delete;
	PrefabCatalog& operator=(const PrefabCatalog&) = delete;

	bool Contains(const std::string& id) const;

	// idのPrefabから生成する。未登録ならnullptr。positionを省略するとPrefabのposを使う。
	GameObject* Spawn(ObjectManager& objectManager, const std::string& id) const;
	GameObject* Spawn(ObjectManager& objectManager, const std::string& id, const Math::Vector3& position) const;

private:
	std::unordered_map<std::string, PrefabDefinition> definitions_;
};
