// マップ上に置くデータ専用コンポーネントの登録。

#include "Application/Factories/ComponentRegistry.h"
#include "Application/Factories/Registrations/RegisterFunctions.h"
#include "Application/Definitions/Map/SpawnMarker.h"

void RegisterMapComponents(ComponentRegistry& registry)
{
	// 実体は作らない。paramsはMapLoaderがSpawnMarkerDefへ回収する
	registry.AddWithParams<SpawnMarkerParams>("SpawnMarker", [](BuildContext&, const SpawnMarkerParams&) {});
}
