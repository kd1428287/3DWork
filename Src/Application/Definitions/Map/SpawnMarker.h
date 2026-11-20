#pragma once
#include "MapData.h"

// 未知の値は先頭(None)になる。typoでPlayer扱いにならないよう、Noneを先頭に置く。
enum class SpawnRole { None, Player, Enemy };

// マップ上に置くスポーン地点。prefabはPrefab(JSON)のパスで、Playerは空なら既定のPlayer.jsonを使う。
struct SpawnMarkerParams
{
	SpawnRole   role = SpawnRole::None;
	std::string prefab;
};

// MapLoaderが回収するスポーン情報。生成はGameSceneが行う。
struct SpawnMarkerDef
{
	ObjectId         id = kInvalidObjectId;
	SpawnRole        role = SpawnRole::None;
	std::string      prefab;
	Math::Vector3    position;
	Math::Quaternion rotation;
};
