#pragma once

#include "Application/Definitions/Prefab/Prefab.h"

// PrefabDefinitionのJSON入出力。実行時(PrefabFactory)とエディタが同じ関数を使う

// 保存形式のJSON。空のchildren配列と空のparamsは省いて差分を小さく保つ
nlohmann::json PrefabToJson(const PrefabDefinition& def);

// 失敗時はfalseを返し、outは変更しない
bool LoadPrefabFile(const std::string& path, PrefabDefinition& out);
bool SavePrefabFile(const std::string& path, const PrefabDefinition& def);
