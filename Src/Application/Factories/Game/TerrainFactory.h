#pragma once

#include "../GameObjectFactory.h"

class TerrainFactory
{
public:
	TerrainFactory() = default;
	~TerrainFactory() = default;

	// コピー・ムーブ禁止
	TerrainFactory(const TerrainFactory&) = delete;
	TerrainFactory& operator=(const TerrainFactory&) = delete;

private:

};