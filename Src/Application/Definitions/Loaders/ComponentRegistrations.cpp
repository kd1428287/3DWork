#include "ComponentRegistry.h"
#include "DefinitionJson.h"

#include "Application/Components/GamePlay/Character/Common/HitReactionComponent.h"

// 新しいコンポーネントは、ここに1行足せばPrefabのtypeから使えるようになる。
void RegisterAllComponents(ComponentRegistry& registry)
{
	registry.Register<HitReactionComponent, HitReactionConfig>("HitReaction");
}
