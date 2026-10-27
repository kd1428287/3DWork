#include "EnemyAttackTableLoader.h"

#include "Application/Definitions/Character/Enemy/EnemyDefinitionJson.h" // EnemyAttackDefinitionのJSON変換
#include "Application/Definitions/Loaders/DefinitionJson.h" // JsonLoader(PrefabFactory::LoadFromFile等と同じ入手経路)

// EnemyAttackTable自体はComponentRegistryのdefaultParamsとしては使わない(ファイルからの
// 読み込み専用)ので、表示順を保持するordered_json版は不要。通常のマクロのみでよい
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EnemyAttackTable, attacks, gapCloserAttacks)

bool EnemyAttackTableLoader::LoadFromFile(const std::string& path, EnemyAttackTable& out)
{
	nlohmann::json root;
	if (!JsonLoader::Load(path, root)) return false;

	try {
		out = root.get<EnemyAttackTable>();
		// エディタで直せるよう、整合性エラーは警告だけにして読み込みは成功させる
		if (!out.Validate()) {
			OutputDebugStringA(("EnemyAttackTableLoader: " + path + ": attack idの重複、またはcomboLinksの参照先不在\n").c_str());
		}
		return true;
	}
	catch (const std::exception& e) {
		OutputDebugStringA(("EnemyAttackTableLoader: " + path + ": " + e.what() + "\n").c_str());
		return false;
	}
}
