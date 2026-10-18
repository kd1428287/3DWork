#include "ComponentRegistry.h"
#include "Application/Factories/Registrations/RegisterFunctions.h"

// 全コンポーネントの登録をカテゴリ別のRegister*()に振り分ける。
// 新しいコンポーネントは該当カテゴリのRegistrations/Register*.cppに足す(Add系の使い分けはComponentRegistry.h参照)。
void RegisterAllComponents(ComponentRegistry& registry)
{
	RegisterGraphicsComponents(registry);
	RegisterPhysicsComponents(registry);
	RegisterCombatComponents(registry);
	RegisterPlayerComponents(registry);
	RegisterEnemyComponents(registry);
	RegisterUIComponents(registry);
	RegisterMapComponents(registry);
}
