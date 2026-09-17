#include "PlayerAttackTableLoader.h"

#include <nlohmann/json.hpp>

#include "JsonLoader.h"
#include "AttackDataLoader.h"

namespace
{
	using json = nlohmann::json;

	ActionCommand ToActionCommand(const std::string& name)
	{
		if (name == "Evade") return ActionCommand::Evade;
		return ActionCommand::Attack;
	}

	void ReadComboLinks(const json& arr, std::vector<ComboLink>& out)
	{
		for (const auto& l : arr) {
			ComboLink link;
			link.requiredCommand = ToActionCommand(l.value("requiredCommand", std::string("Attack")));
			link.priority = l.value("priority", 0);
			link.nextAttackId = l.value("nextAttackId", std::string());
			out.push_back(link);
		}
	}

	PlayerAttackDefinition ReadPlayerAttackDefinition(const json& j)
	{
		PlayerAttackDefinition def;
		def.id = j.value("id", std::string());

		// AttackData本体(ダメージ/移動/フェーズ/キャンセル/武器スロット)は
		// Player/Enemy共通のため、AttackDataLoaderへ委譲する
		// (このファイルが持つのは、コンボ木/entryCommandというPlayer固有の
		// ラッパー部分の読み方だけにする)。
		if (j.contains("attack")) AttackDataLoader::ReadAttackData(j["attack"], def.attack);
		if (j.contains("comboLinks")) ReadComboLinks(j["comboLinks"], def.comboLinks);

		// entryCommandは「キー自体が無い/null」ならコンボ中継専用の技として
		// 明示的にnulloptにする(PlayerAttackDefinitionのデフォルト値
		// ActionCommand::Attackのまま残すと、全ての技がIdleから出せる
		// 攻撃として扱われてしまうため)。
		if (j.contains("entryCommand") && !j["entryCommand"].is_null()) {
			def.entryCommand = ToActionCommand(j["entryCommand"].get<std::string>());
		}
		else {
			def.entryCommand = std::nullopt;
		}

		return def;
	}
}

bool PlayerAttackTableLoader::LoadFromFile(const std::string& path, PlayerAttackTable& outTable)
{
	json root;
	if (!JsonLoader::Load(path, root)) return false;

	if (!root.contains("attacks")) return false;

	PlayerAttackTable table;
	for (const auto& a : root["attacks"]) {
		table.attacks.push_back(ReadPlayerAttackDefinition(a));
	}

	outTable = std::move(table);
	return true;
}
