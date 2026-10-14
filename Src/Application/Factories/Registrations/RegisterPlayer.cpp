// Player専用のコンポーネント登録。

#include "Application/Factories/ComponentRegistry.h"
#include "Application/Factories/Registrations/RegisterFunctions.h"
#include "Application/Factories/Registrations/RegistrationHelpers.h"
#include "Application/Definitions/Loaders/ComponentParamsOrderedJson.h"
#include "Application/Definitions/Loaders/DefinitionJson.h"
#include "Application/Definitions/Loaders/PlayerAttackTableLoader.h"
#include "Application/Definitions/Character/Player/PlayerDefinitionJson.h"

#include "Application/Components/GamePlay/Character/Player/PlayerComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerAttackSelector.h"
#include "Application/Components/GamePlay/Character/Player/PlayerCombatMovementComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerFacingComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerInputComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerLockOnComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerMovementAnimationComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerStatusController.h"

using namespace Registration;

COMPONENT_PARAMS_DEFINE_TYPE(PlayerCombatMovementConfig, walkSpeed, runSpeed)
COMPONENT_PARAMS_DEFINE_TYPE(PlayerStatusControllerConfig, evade, guard, charge, stagger)

// 接続を伴う型のparams。ODR違反を避けるため、マクロも含めて無名名前空間に入れる。
namespace
{
	struct PlayerAttackSelectorParams
	{
		std::string attackTablePath;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(PlayerAttackSelectorParams, attackTablePath)

	// コンボ木は専用ファイルから読む。読み込みに失敗しても、空のテーブルでコンポーネントは追加する。
	void BuildPlayerAttackSelector(BuildContext& ctx, const PlayerAttackSelectorParams& p)
	{
		PlayerAttackTable table;
		if (!PlayerAttackTableLoader::LoadFromFile(p.attackTablePath, table)) {
			OutputDebugStringA(("PlayerAttackSelector: attack table load failed: " + p.attackTablePath + "\n").c_str());
		}
		ctx.Add<PlayerAttackSelector>()->SetAttackTable(table);
	}
}  // namespace

void RegisterPlayerComponents(ComponentRegistry& registry)
{
	registry.Add<PlayerComponent>("Player");
	registry.Add<PlayerInputComponent>("PlayerInput");
	registry.Add<PlayerLockOnComponent>("PlayerLockOn");
	registry.Add<PlayerFacingComponent>("PlayerFacing");
	registry.Add<PlayerMovementAnimationComponent>("PlayerMovementAnimation");
	registry.Add<PlayerStatusController>("PlayerStatusController");
	registry.AddWithParams<PlayerCombatMovementConfig>("PlayerCombatMovement", [](BuildContext& ctx, const PlayerCombatMovementConfig& config) {
		// 速度と、踏み込み距離など実行時に渡される距離の両方をモデルの拡大率に合わせる。
		const float scale = HorizontalScale(ctx);
		PlayerCombatMovementConfig scaled = config;
		scaled.walkSpeed *= scale;
		scaled.runSpeed *= scale;
		scaled.lengthScale = scale;
		ctx.Add<PlayerCombatMovementComponent>()->SetConfig(scaled);
		});
	registry.AddWithParams<PlayerAttackSelectorParams>("PlayerAttackSelector", BuildPlayerAttackSelector);
}
