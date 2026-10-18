#include "KdSoundDef.h"

#include <fstream>
#include <nlohmann/json.hpp>	// プロジェクトのJSONライブラリに合わせて変更する

namespace
{
	SoundCategory ParseCategory(const std::string& str)
	{
		if (str == "BGM")	{ return SoundCategory::BGM; }
		if (str == "Voice")	{ return SoundCategory::Voice; }

		return SoundCategory::SE;
	}
}

bool KdSoundDefTable::Load(std::string_view path)
{
	std::ifstream ifs{ std::string(path) };

	if (!ifs)
	{
		assert(0 && "SoundDef:ファイルを開けません。パスを確認してください。");

		return false;
	}

	nlohmann::json root;

	try
	{
		root = nlohmann::json::parse(ifs);
	}
	catch (...)
	{
		assert(0 && "SoundDef:JSONの形式が不正です。");

		return false;
	}

	if (!root.is_object()) { return false; }

	m_defs.clear();

	for (auto& [id, j] : root.items())
	{
		SoundDef def;

		// fileは文字列でも配列でもよい
		if (j.contains("file"))
		{
			if (j["file"].is_array())
			{
				for (auto& f : j["file"]) { def.files.push_back(f.get<std::string>()); }
			}
			else
			{
				def.files.push_back(j["file"].get<std::string>());
			}
		}

		if (def.files.empty())
		{
			assert(0 && "SoundDef:fileが未指定のIDがあります。");

			continue;
		}

		def.category = ParseCategory(j.value("category", "SE"));
		def.volume = j.value("volume", 1.0f);
		def.pitchRandom = j.value("pitchRandom", 0.0f);
		def.maxInstances = j.value("maxInstances", 0);
		def.loop = j.value("loop", false);

		m_defs[id] = std::move(def);
	}

	return true;
}

const SoundDef* KdSoundDefTable::Find(const std::string& soundId) const
{
	auto it = m_defs.find(soundId);

	if (it == m_defs.end()) { return nullptr; }

	return &it->second;
}
