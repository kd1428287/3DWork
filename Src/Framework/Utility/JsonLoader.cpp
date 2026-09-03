#include "../../Application/main.h"
#include "JsonLoader.h"

#include <fstream>

bool JsonLoader::Load(const std::string& path, nlohmann::json& out)
{
	std::ifstream ifs(path);
	if (!ifs) { return false; }

	try
	{
		ifs >> out;
	}
	catch (const nlohmann::json::parse_error&)
	{
		// 壊れたJSON/空ファイル等。呼び出し側には読み込み失敗として通知する
		return false;
	}

	return true;
}

bool JsonLoader::Save(const std::string& path, const nlohmann::json& j)
{
	std::ofstream ofs(path);
	if (!ofs) { return false; }

	ofs << j.dump(2);

	return true;
}

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