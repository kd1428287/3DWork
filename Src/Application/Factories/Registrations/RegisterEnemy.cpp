// Enemy専用のコンポーネント登録。

#include "Application/Factories/ComponentRegistry.h"
#include "Application/Factories/Registrations/RegisterFunctions.h"
#include "Application/Factories/Registrations/RegistrationHelpers.h"
#include "Application/Definitions/Loaders/ComponentParamsOrderedJson.h"
#include "Application/Definitions/Loaders/DefinitionJson.h"
#include "Application/Definitions/Loaders/EnemyAttackTableLoader.h"
#include "Application/Definitions/Character/Enemy/EnemyDefinitionJson.h"

#include "Application/Components/GamePlay/Character/Enemy/EnemyAIController.h"
#include "Application/Components/GamePlay/Character/Enemy/Warrock/WarrockBehavior.h"
#include "Application/Components/GamePlay/Character/Enemy/LockOnTargetComponent.h"

using namespace Registration;

// 接続を伴う型のparams。ODR違反を避けるため、マクロも含めて無名名前空間に入れる。
namespace
{
	// Enemy1体分の生成パラメータ。dataは挙動のチューニング値(EnemyAIData)、
	// behaviorは実際の行動ロジック(IEnemyBehaviorの具象クラス)を選ぶキー。
	// attackTablePathを指定すると、data.attacks/gapCloserAttacksはそちらの内容で
	// 上書きされる(PlayerAttackSelector.attackTablePathと同じ考え方。
	// 未指定ならdataに直接書かれたattacks/gapCloserAttacksをそのまま使う)。
	// 対応する具象クラスが増えたら、下のCreateEnemyBehavior()に足すこと。
	struct EnemyAIControllerParams
	{
		std::string behavior;
		std::string attackTablePath;
		EnemyAIData data;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(EnemyAIControllerParams, behavior, attackTablePath, data)

	std::unique_ptr<IEnemyBehavior> CreateEnemyBehavior(const std::string& behavior)
	{
		if (behavior == "Warrock") return std::make_unique<WarrockBehavior>();
		throw std::runtime_error("EnemyAIController: unknown behavior \"" + behavior + "\"");
	}

	// EnemyAIControllerはGameObject*/EnemyAIData/IEnemyBehaviorの3つをコンストラクタで
	// 要求しSetConfig()も持たないため、Add<T>()には乗らない(EnemyAIController.h冒頭コメント参照)
	void BuildEnemyAIController(BuildContext& ctx, const EnemyAIControllerParams& p)
	{
		EnemyAIData data = p.data;

		if (!p.attackTablePath.empty()) {
			EnemyAttackTable table;
			if (EnemyAttackTableLoader::LoadFromFile(p.attackTablePath, table)) {
				data.attacks = std::move(table.attacks);
				data.gapCloserAttacks = std::move(table.gapCloserAttacks);
			}
			else {
				OutputDebugStringA(("EnemyAIController: attack table load failed: " + p.attackTablePath + "\n").c_str());
			}
		}

		ctx.Add<EnemyAIController>(data, CreateEnemyBehavior(p.behavior));
	}
}  // namespace

void RegisterEnemyComponents(ComponentRegistry& registry)
{
	registry.AddWithParams<EnemyAIControllerParams>("EnemyAIController", BuildEnemyAIController);
	registry.Add<LockOnTargetComponent>("LockOnTarget");
}
