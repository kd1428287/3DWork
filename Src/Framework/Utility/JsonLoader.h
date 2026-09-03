#pragma once
#pragma once

#include "nlohmann/json.hpp"
#include <string>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 汎用JSONファイル入出力ユーティリティ
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 特定のデータ構造には依存せず、ファイルI/O(読み込み/書き込み/更新日時取得)のみを担当する。
// エフェクトデータに限らず、JSONで保存するデータ全般から利用できる想定。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class JsonLoader
{
public:

	// pathからJSONを読み込みoutへ格納する。ファイルが無い/パースに失敗した場合はfalse(outは変更しない)
	static bool Load(const std::string& path, nlohmann::json& out);

	// pathへJSONを書き出す(インデント2)。書き込みに失敗した場合はfalse
	static bool Save(const std::string& path, const nlohmann::json& j);

	// ファイルの最終更新日時を取得する(ホットリロード検知用)。存在しない場合はfalse
	static bool GetLastWriteTime(const std::string& path, FILETIME& out);
};