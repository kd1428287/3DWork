#include "MapData.h"
#include <unordered_set>
#include <algorithm>

namespace
{
	constexpr int kCurrentVersion = 2;

	Math::Vector3 GetVec3(const nlohmann::json& j, const char* key, const Math::Vector3& def)
	{
		if (!j.contains(key)) return def;
		auto& v = j[key];
		if (!v.is_array() || v.size() < 3) return def;
		return Math::Vector3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
	}

	nlohmann::json Vec3ToJson(const Math::Vector3& v)
	{
		return { v.x, v.y, v.z };
	}

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// v1(バージョン管理導入前) → v2 への変換。
	//	v1はルート直下が配列で、回転はEuler角(度)、モデルは"model"直書き
	//	または初期のcomponents配列。v2はルートがオブジェクトでversion付き、
	//	回転はQuaternion[x,y,z,w]、componentsは必須。
	//	idはv1に存在しないため、ここで新規採番する。
	//
	//	新しいバージョンを追加する時は、この関数のすぐ下に同じ形で
	//	MigrateVNtoVN+1を足し、MigrateToLatest()のdispatchに1行追加するだけでよい
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	nlohmann::json MigrateV1ToV2(const nlohmann::json& rootArray)
	{
		nlohmann::json entities = nlohmann::json::array();
		ObjectId nextId = 1;

		for (auto& e : rootArray)
		{
			nlohmann::json entity;
			entity["id"] = nextId++;
			entity["name"] = e.value("name", std::string("Object"));
			entity["pos"] = e.contains("pos") ? e["pos"] : nlohmann::json::array({ 0.0f, 0.0f, 0.0f });

			Math::Vector3 rotDeg = GetVec3(e, "rotate", { 0.0f, 0.0f, 0.0f });
			Math::Quaternion q = Math::Quaternion::CreateFromYawPitchRoll(
				DirectX::XMConvertToRadians(rotDeg.y),
				DirectX::XMConvertToRadians(rotDeg.x),
				DirectX::XMConvertToRadians(rotDeg.z));
			entity["rotation"] = { q.x, q.y, q.z, q.w };

			entity["scale"] = e.contains("scale") ? e["scale"] : nlohmann::json::array({ 1.0f, 1.0f, 1.0f });

			nlohmann::json components = nlohmann::json::array();
			if (e.contains("components") && e["components"].is_array())
			{
				components = e["components"];
			}
			else if (e.contains("model"))
			{
				std::string modelPath = e.value("model", std::string());
				if (!modelPath.empty())
				{
					components.push_back({
						{ "type", "ModelRender" },
						{ "params", { { "model", modelPath } } }
						});
				}
			}
			entity["components"] = components;

			entities.push_back(std::move(entity));
		}

		nlohmann::json root;
		root["version"] = 2;
		root["entities"] = entities;
		return root;
	}

	// rootを現行バージョンまで変換する。バージョン管理導入前(ルートが配列)の
	// ファイルも含め、どんな旧形式で来てもここを通せば現行スキーマになる
	void MigrateToLatest(nlohmann::json& root)
	{
		if (root.is_array())
		{
			root = MigrateV1ToV2(root);
		}

		// 現状はv1(配列)→v2の1段のみ。将来バージョンが増えたら
		// int version = root.value("version", 1);
		// if (version == 2) { root = MigrateV2ToV3(root); version = 3; }
		// のように version を見ながら鎖状に適用していく

		root["version"] = kCurrentVersion;
	}

	bool ParseEntity(const nlohmann::json& e, MapEntity& out)
	{
		try
		{
			out.id = e.value("id", (ObjectId)kInvalidObjectId);
			out.name = e.value("name", std::string("Object"));
			out.pos = GetVec3(e, "pos", { 0.0f, 0.0f, 0.0f });
			out.scale = GetVec3(e, "scale", { 1.0f, 1.0f, 1.0f });

			out.rotation = Math::Quaternion::Identity;
			if (e.contains("rotation") && e["rotation"].is_array() && e["rotation"].size() >= 4)
			{
				auto& r = e["rotation"];
				out.rotation = Math::Quaternion(
					r[0].get<float>(), r[1].get<float>(), r[2].get<float>(), r[3].get<float>());
			}

			out.components.clear();
			if (e.contains("components") && e["components"].is_array())
			{
				for (auto& c : e["components"])
				{
					ComponentEntry entry;
					entry.type = c.value("type", std::string());
					if (entry.type.empty()) continue;	// typeの無いエントリは無視

					entry.params = c.value("params", nlohmann::json::object());
					out.components.push_back(std::move(entry));
				}
			}
		}
		catch (const nlohmann::json::exception&)
		{
			return false;
		}

		return true;
	}

	nlohmann::json EntityToJson(const MapEntity& entity)
	{
		nlohmann::json componentsJson = nlohmann::json::array();
		for (auto& c : entity.components)
		{
			componentsJson.push_back({
				{ "type",   c.type },
				{ "params", c.params }
				});
		}

		return {
			{ "id",         entity.id },
			{ "name",       entity.name },
			{ "pos",        Vec3ToJson(entity.pos) },
			{ "rotation",   { entity.rotation.x, entity.rotation.y, entity.rotation.z, entity.rotation.w } },
			{ "scale",      Vec3ToJson(entity.scale) },
			{ "components", componentsJson }
		};
	}
}

bool LoadMapFile(const std::string& path, MapFile& out)
{
	nlohmann::json root;
	if (!JsonLoader::Load(path, root))
	{
		return false;
	}

	MigrateToLatest(root);

	std::vector<MapEntity> parsed;

	if (root.contains("entities") && root["entities"].is_array())
	{
		for (auto& e : root["entities"])
		{
			MapEntity entity;
			if (!ParseEntity(e, entity)) continue;	// 個別エンティティの破損はスキップして続行
			parsed.push_back(std::move(entity));
		}
	}

	// id==0(欠落)や重複したidは、ここで安全な値に採番し直す。
	// (手で編集されたJSON等、想定外の入力でもクラッシュせず一意なIDを保証するため)
	ObjectId maxStatedId = 0;
	for (auto& entity : parsed) { maxStatedId = std::max(maxStatedId, entity.id); }

	std::unordered_set<ObjectId> used;
	ObjectId nextFresh = maxStatedId + 1;

	for (auto& entity : parsed)
	{
		if (entity.id == kInvalidObjectId || used.count(entity.id))
		{
			entity.id = nextFresh++;
		}
		used.insert(entity.id);
	}

	MapFile result;
	result.entities = std::move(parsed);
	result.nextId = nextFresh;

	out = std::move(result);
	return true;
}

bool SaveMapFile(const std::string& path, const std::vector<MapEntity>& entities)
{
	nlohmann::json root;
	root["version"] = kCurrentVersion;

	nlohmann::json entitiesJson = nlohmann::json::array();
	for (auto& entity : entities)
	{
		entitiesJson.push_back(EntityToJson(entity));
	}
	root["entities"] = entitiesJson;

	return JsonLoader::Save(path, root);
}