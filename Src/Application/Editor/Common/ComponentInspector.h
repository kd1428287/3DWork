#pragma once

#include <functional>
#include <string>
#include <vector>

#include "nlohmann/json_fwd.hpp"
#include "Application/Definitions/Map/MapData.h"	// ComponentEntryの定義

// ComponentEntryの配列(=オブジェクトが持つコンポーネント構成)を編集する汎用Inspector UI。
class ComponentInspector
{
public:

	using RequestUndoFn = std::function<void()>;
	using OnEditedFn = std::function<void(const std::string& componentType)>;

	// コンポーネント一覧(追加/削除ボタン + 各コンポーネントのparams編集UI)を描画する
	static void DrawList(
		std::vector<ComponentEntry>& components,
		const RequestUndoFn& requestUndoCheckpoint,
		const OnEditedFn& onComponentEdited = nullptr);
};