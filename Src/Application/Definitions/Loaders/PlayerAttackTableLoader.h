#pragma once
#include "Application/Definitions/Character/Player/PlayerCombatTypes.h"

// ============================================================
// PlayerAttackTable(コンボ木)専用のファイルI/O。
// ============================================================
namespace PlayerAttackTableLoader
{
	// path 例: "Asset/Data/Player/PlayerAttackTable.json"
	// 読み込み・パースに失敗した場合はfalseを返し、outTableは変更しない。
	bool LoadFromFile(const std::string& path, PlayerAttackTable& outTable);

	// outTableをpathへ書き出す(アクションエディタの保存用)。既存ファイルの"_comment"は残す。
	// 書き込みに失敗した場合はfalseを返す。
	bool SaveToFile(const std::string& path, const PlayerAttackTable& table);
}
