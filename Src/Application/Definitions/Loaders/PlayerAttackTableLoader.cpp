#include "PlayerAttackTableLoader.h"

#include <nlohmann/json.hpp>

#include "JsonLoader.h"
#include "DefinitionJson.h"

// 未知のコマンド名は先頭のAttackになる。
NLOHMANN_JSON_SERIALIZE_ENUM(ActionCommand, {
	{ ActionCommand::Attack, "Attack" },
	{ ActionCommand::Evade,  "Evade" },
	{ ActionCommand::Guard, "Guard" },
	{ ActionCommand::ChargeAttack, "ChargeAttack" },
	})

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ComboLink, requiredCommand, priority, nextAttackId)

	// entryCommandはキー無し/nullを「コンボ中継専用(nullopt)」として読む。structの既定値(Attack)と異なるためマクロにしない。
	static void from_json(const nlohmann::json& j, PlayerAttackDefinition& def)
{
	def.id = j.value("id", std::string());
	def.attack = j.value("attack", AttackData());
	def.comboLinks = j.value("comboLinks", std::vector<ComboLink>());

	def.entryCommand = std::nullopt;
	if (j.contains("entryCommand") && !j["entryCommand"].is_null()) {
		def.entryCommand = j["entryCommand"].get<ActionCommand>();
	}
}

bool PlayerAttackTableLoader::LoadFromFile(const std::string& path, PlayerAttackTable& outTable)
{
	nlohmann::json root;
	if (!JsonLoader::Load(path, root)) return false;

	if (!root.contains("attacks")) return false;

	// 型不一致等は例外になるため、失敗(false)として扱う。
	try {
		PlayerAttackTable table;
		table.attacks = root["attacks"].get<std::vector<PlayerAttackDefinition>>();

		outTable = std::move(table);
		return true;
	}
	catch (const std::exception& e) {
		OutputDebugStringA(("PlayerAttackTableLoader: " + path + ": " + e.what() + "\n").c_str());
		return false;
	}
}

bool PlayerAttackTableLoader::SaveToFile(const std::string& path, const PlayerAttackTable& table)
{
	nlohmann::ordered_json root;

	// 既存ファイルの_commentだけ引き継ぐ
	nlohmann::ordered_json old;
	if (JsonLoader::Load(path, old) && old.contains("_comment")) { root["_comment"] = old["_comment"]; }

	// ordered_jsonで書く(キー順をstructの定義順に保つ)。comboLinks/entryCommandは手で組む
	root["attacks"] = nlohmann::ordered_json::array();
	for (const PlayerAttackDefinition& def : table.attacks)
	{
		nlohmann::ordered_json j;
		j["id"] = def.id;
		j["attack"] = def.attack;

		nlohmann::ordered_json links = nlohmann::ordered_json::array();
		for (const ComboLink& link : def.comboLinks)
		{
			nlohmann::ordered_json l;
			l["requiredCommand"] = link.requiredCommand;
			l["priority"] = link.priority;
			l["nextAttackId"] = link.nextAttackId;
			links.push_back(l);
		}
		j["comboLinks"] = links;

		// nullなら読み込み時にnullopt(コンボ中継専用)になる
		if (def.entryCommand.has_value()) { j["entryCommand"] = def.entryCommand.value(); }
		else { j["entryCommand"] = nullptr; }

		root["attacks"].push_back(j);
	}

	return JsonLoader::Save(path, root);
}
