#pragma once

#include <string>

#include "PlayerDefinition.h"

// ============================================================
// PlayerDefinitionのファイルI/Oだけを担当するローダー。
// PlayerFactoryはこのローダーが返したPlayerDefinitionを使って
// GameObjectを組み立てるだけにし、パース処理には関与しない。
// ============================================================
namespace PlayerDefinitionLoader
{
	// path 例: "Asset/Data/Player/Player.json"
	// 読み込み・パースに失敗した場合は false を返し、outDefinitionは変更しない。
	bool LoadFromFile(const std::string& path, PlayerDefinition& outDefinition);
}
