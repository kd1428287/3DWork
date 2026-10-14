#include "UIData.h"

#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace
{
	constexpr int kUIFileVersion = 2;
	constexpr int kManifestVersion = 1;

	// 保存先のフォルダが無ければ作る
	void EnsureParentDirectory(const std::string& path)
	{
		std::error_code ec;
		std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
	}

	// 不正なUTF-8が混ざると保存で例外になるため、置換文字に直したJSONにする
	nlohmann::json ReplaceInvalidUtf8(const nlohmann::json& j)
	{
		return nlohmann::json::parse(j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace));
	}

	// 旧バージョンのファイルを現行スキーマへ変換する。
	//	今はv2のみなのでversionを揃えるだけ。バージョンを増やす時は、versionを見ながら
	//	MigrateVNtoVN+1を鎖状に適用する(MapData.cppのMigrateToLatestと同じ作り)
	void MigrateToLatest(nlohmann::json& root)
	{
		root["version"] = kUIFileVersion;
	}

	nlohmann::json ElementToJson(const UIElement& e)
	{
		nlohmann::json comps = nlohmann::json::array();
		for (auto& c : e.components)
		{
			comps.push_back({
				{ "type",   c.type },
				{ "params", c.params }
				});
		}

		return {
			{ "id",         e.id },
			{ "parent",     e.parentId },
			{ "name",       e.name },
			{ "visible",    e.visible },
			{ "components", comps }
		};
	}

	bool ParseElement(const nlohmann::json& j, UIElement& out)
	{
		try
		{
			if (!j.is_object()) return false;

			out.id = j.value("id", 0u);
			out.parentId = j.value("parent", 0u);
			out.name = j.value("name", std::string());
			out.visible = j.value("visible", true);

			out.components.clear();
			if (j.contains("components") && j["components"].is_array())
			{
				for (auto& jc : j["components"])
				{
					ComponentEntry c;
					c.type = jc.value("type", std::string());
					if (c.type.empty()) continue;	// typeの無いエントリは無視

					c.params = (jc.contains("params") && jc["params"].is_object()) ? jc["params"] : nlohmann::json::object();
					out.components.push_back(std::move(c));
				}
			}
		}
		catch (const nlohmann::json::exception&)
		{
			return false;
		}

		return true;
	}

	// idが0・重複の要素は新しいidに振り直し、存在しない親・自分自身・循環した親は最上位へ戻す。
	// (手で編集されたJSON等、想定外の入力でもクラッシュせず一意なIDと正しい階層を保証するため)
	void RepairElements(UIFile& file, UIId statedNextId)
	{
		UIId maxId = 0;
		for (auto& e : file.elements) { maxId = (std::max)(maxId, e.id); }

		UIId nextFresh = (std::max)(statedNextId, maxId + 1);

		std::unordered_set<UIId> used;
		for (auto& e : file.elements)
		{
			if (e.id == kInvalidUIId || used.count(e.id)) { e.id = nextFresh++; }
			used.insert(e.id);
		}
		file.nextId = nextFresh;

		std::unordered_map<UIId, UIId> parentOf;
		for (auto& e : file.elements) { parentOf[e.id] = e.parentId; }

		for (auto& e : file.elements)
		{
			if (e.parentId == kInvalidUIId) continue;

			// 親を辿って最上位へ着けば正常
			bool ok = false;
			UIId cur = e.parentId;
			for (size_t step = 0; step <= file.elements.size(); ++step)
			{
				if (cur == kInvalidUIId) { ok = true; break; }
				if (cur == e.id) break;

				auto it = parentOf.find(cur);
				if (it == parentOf.end()) break;
				cur = it->second;
			}

			if (!ok)
			{
				e.parentId = kInvalidUIId;
				parentOf[e.id] = kInvalidUIId;
			}
		}
	}
}

nlohmann::json UIFileToJson(const UIFile& file)
{
	nlohmann::json elements = nlohmann::json::array();
	for (auto& e : file.elements)
	{
		elements.push_back(ElementToJson(e));
	}

	nlohmann::json root;
	root["version"] = kUIFileVersion;
	root["refSize"] = { file.refSize.x, file.refSize.y };
	root["nextId"] = file.nextId;
	root["elements"] = std::move(elements);
	return root;
}

bool LoadUIFile(const std::string& path, UIFile& out)
{
	nlohmann::json root;
	if (!JsonLoader::Load(path, root)) return false;
	if (!root.is_object()) return false;

	MigrateToLatest(root);

	UIFile file;
	file.refSize = UIDataDetail::ReadVec2(root, "refSize", file.refSize);

	if (root.contains("elements") && root["elements"].is_array())
	{
		for (auto& j : root["elements"])
		{
			UIElement e;
			if (!ParseElement(j, e)) continue;	// 個別の要素の破損はスキップして続行
			file.elements.push_back(std::move(e));
		}
	}

	const UIId statedNextId = (root.contains("nextId") && root["nextId"].is_number_unsigned())
		? root["nextId"].get<UIId>() : 1;
	RepairElements(file, statedNextId);

	out = std::move(file);
	return true;
}

bool SaveUIFile(const std::string& path, const UIFile& file)
{
	nlohmann::json root = ReplaceInvalidUtf8(UIFileToJson(file));

	EnsureParentDirectory(path);
	return JsonLoader::Save(path, root);
}

bool LoadUIManifest(const std::string& path, UIManifest& out)
{
	nlohmann::json root;
	if (!JsonLoader::Load(path, root)) return false;
	if (!root.is_object()) return false;

	UIManifest manifest;
	if (root.contains("screens") && root["screens"].is_array())
	{
		for (auto& j : root["screens"])
		{
			if (!j.is_object()) continue;
			if (!j.contains("name") || !j["name"].is_string()) continue;
			if (!j.contains("path") || !j["path"].is_string()) continue;

			UIScreenEntry e;
			e.name = j["name"].get<std::string>();
			e.path = j["path"].get<std::string>();
			if (e.name.empty() || e.path.empty()) continue;
			if (manifest.Find(e.name) >= 0) continue;	// 名前の重複は先のものを優先

			manifest.screens.push_back(std::move(e));
		}
	}

	out = std::move(manifest);
	return true;
}

bool SaveUIManifest(const std::string& path, const UIManifest& manifest)
{
	nlohmann::json screens = nlohmann::json::array();
	for (auto& s : manifest.screens)
	{
		screens.push_back({
			{ "name", s.name },
			{ "path", s.path }
			});
	}

	nlohmann::json root;
	root["version"] = kManifestVersion;
	root["screens"] = std::move(screens);
	root = ReplaceInvalidUtf8(root);

	EnsureParentDirectory(path);
	return JsonLoader::Save(path, root);
}
