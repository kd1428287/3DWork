#pragma once

#include <string>
#include <vector>

#include "Application/Definitions/Effect/SlashTrailParams.h"

// トレイル定義ファイル(JSON)の1件分
struct SlashTrailDefEntry
{
	std::string			Name;
	SlashTrailParams	Params;
};

// トレイル定義ファイル全体。形式：{ "Trails": [ { "Name": "...", "Params": { ... } } ] }
struct SlashTrailDataFile
{
	std::vector<SlashTrailDefEntry>	Trails;
};

// SlashTrailDispatcher(読み込み)とSlashTrailEditor(保存)で共有するJSON入出力
namespace SlashTrailDataLoader
{
	// 読めなければfalse(ファイル無し・パース失敗)
	bool Load(const std::string& path, SlashTrailDataFile& out);

	bool Save(const std::string& path, const SlashTrailDataFile& data);
}
