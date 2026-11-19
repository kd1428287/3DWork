#include "Application/main.h"
#include "SlashTrailDataLoader.h"
#include "../Effect/SlashTrailParamsJson.h"

#include <filesystem>
#include <fstream>

namespace SlashTrailDataLoader
{
	bool Load(const std::string& path, SlashTrailDataFile& out)
	{
		out.Trails.clear();

		std::ifstream ifs(path);
		if (!ifs) { return false; }

		const nlohmann::ordered_json root = nlohmann::ordered_json::parse(ifs, nullptr, false);
		if (root.is_discarded() || !root.is_object()) { return false; }

		const auto it = root.find("Trails");
		if (it == root.end() || !it->is_array()) { return true; }

		for (const auto& entry : *it)
		{
			if (!entry.is_object() || !entry.contains("Name")) { continue; }

			SlashTrailDefEntry def;
			def.Name = entry["Name"].get<std::string>();
			if (def.Name.empty()) { continue; }

			// Paramsが無い・一部キーが無い場合はstructの既定値になる
			if (entry.contains("Params")) { def.Params = entry["Params"].get<SlashTrailParams>(); }

			out.Trails.push_back(std::move(def));
		}

		return true;
	}

	bool Save(const std::string& path, const SlashTrailDataFile& data)
	{
		nlohmann::ordered_json root;
		root["Trails"] = nlohmann::ordered_json::array();

		for (const auto& def : data.Trails)
		{
			nlohmann::ordered_json entry;
			entry["Name"] = def.Name;
			entry["Params"] = def.Params;
			root["Trails"].push_back(std::move(entry));
		}

		const std::filesystem::path p(path);
		if (p.has_parent_path()) { std::filesystem::create_directories(p.parent_path()); }

		std::ofstream ofs(path);
		if (!ofs) { return false; }

		ofs << root.dump(4);
		return ofs.good();
	}
}
