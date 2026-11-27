#pragma once

#include "Application/Definitions/Prefab/Prefab.h"

// id → Prefab定義の対応表。定義の保持と引き当てだけを行い、生成はPrefabSpawnerが担当する。
class PrefabCatalog {
public:
	// pathsは id → Prefab JSONファイルパス の対応表。読み込みに失敗した定義はログに出して飛ばす。
	explicit PrefabCatalog(const std::unordered_map<std::string, std::string>& paths);
	~PrefabCatalog() = default;

	PrefabCatalog(const PrefabCatalog&) = delete;
	PrefabCatalog& operator=(const PrefabCatalog&) = delete;

	bool Contains(const std::string& id) const;

	// idの定義を返す。未登録ならnullptr。
	const PrefabDefinition* Find(const std::string& id) const;

private:
	std::unordered_map<std::string, PrefabDefinition> definitions_;
};
