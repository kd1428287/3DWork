#include "PlayerAttackTableLoader.h"

#include <nlohmann/json.hpp>

#include "JsonLoader.h"
#include "DefinitionJson.h"

// 未知のコマンド名は先頭のAttackになる。
NLOHMANN_JSON_SERIALIZE_ENUM(ActionCommand, {
	{ ActionCommand::Attack, "Attack" },
	{ ActionCommand::Evade,  "Evade" },
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