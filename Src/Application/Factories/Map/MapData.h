#pragma once
#include "ComponentTypes.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップ上に配置する1エンティティ分のデータ
//	MapEditor(MapObject)が保存するJSONスキーマと1:1対応させてある:
//		{ "name":..., "pos":[x,y,z], "rotate":[x,y,z], "scale":[x,y,z],
//		  "components":[ { "type":"...", "params":{...} }, ... ] }
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct TransformData
{
	Math::Vector3 position = { 0.f, 0.f, 0.f };
	Math::Vector3 rotation = { 0.f, 0.f, 0.f };	// オイラー角(度)。MapEditorの rotate と同じ単位
	Math::Vector3 scale = { 1.f, 1.f, 1.f };
};

struct EntityData
{
	std::string					name;			// MapEditor: MapObject::name
	TransformData				transform;
	std::vector<ComponentEntry>	components;		// MapEditor: MapObject::components と同一スキーマ
};

struct MapData
{
	std::vector<EntityData> entities;
};