#pragma once

#include <string>
#include <vector>

#include "Application/Effect/Shape/ShapeEffectInstance.h"	// ShapeEffectParams
#include "Application/Effect/Shape/ShapeEffectMotion.h"		// EffectKey

// JSON上の1エフェクト定義(見た目+キー列)。EditorとDispatcherの共通の中間データ
struct ShapeEffectData
{
	std::string				Name;
	ShapeEffectParams		Params;
	std::vector<EffectKey>	Keys;
};

struct ShapeEffectDataFile
{
	std::vector<ShapeEffectData>	Effects;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ShapeEffect定義JSON("shapeEffects"配列)の読み書き
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// effectmap.jsonとは別ファイル。EffectEditorのSaveがファイル全体を書き直すため、
// 同居させると保存のたびにこちらの内容が消えてしまう。
// 壊れたエントリは警告を出してそのエントリだけ読み飛ばす(EffectDataLoaderと同じ方針)。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class ShapeEffectDataLoader
{
public:

	// 失敗時はfalseを返しoutは変更しない
	static bool Load(const std::string& path, ShapeEffectDataFile& out);

	static bool Save(const std::string& path, const ShapeEffectDataFile& data);
};
