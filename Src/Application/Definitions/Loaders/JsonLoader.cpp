#include "Application/main.h"
#include "JsonLoader.h"

#include <fstream>

// json / ordered_json 共通の実装
template <typename JsonT>
static bool LoadImpl(const std::string& path, JsonT& out)
{
	std::ifstream ifs(path);
	if (!ifs) { return false; }

	try
	{
		ifs >> out;
	}
	catch (const nlohmann::json::exception&)
	{
		// 壊れたJSON/空ファイル等。呼び出し側には読み込み失敗として通知する
		return false;
	}

	return true;
}

template <typename JsonT>
static bool SaveImpl(const std::string& path, const JsonT& j)
{
	std::ofstream ofs(path);
	if (!ofs) { return false; }

	ofs << j.dump(2);

	return true;
}

bool JsonLoader::Load(const std::string& path, nlohmann::json& out) { return LoadImpl(path, out); }
bool JsonLoader::Save(const std::string& path, const nlohmann::json& j) { return SaveImpl(path, j); }
bool JsonLoader::Load(const std::string& path, nlohmann::ordered_json& out) { return LoadImpl(path, out); }
bool JsonLoader::Save(const std::string& path, const nlohmann::ordered_json& j) { return SaveImpl(path, j); }

bool JsonLoader::GetLastWriteTime(const std::string& path, FILETIME& out)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data))
	{
		return false;
	}

	out = data.ftLastWriteTime;
	return true;
}