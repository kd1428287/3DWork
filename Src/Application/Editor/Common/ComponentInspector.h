#pragma once

#include <functional>
#include <string>
#include <vector>

#include "nlohmann/json_fwd.hpp"
#include "Application/Definitions/Map/MapData.h"	// ComponentEntryの定義

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ComponentEntryの配列(=オブジェクトが持つコンポーネント構成)を編集する汎用Inspector UI。
//
//	元々MapEditor専用だったが、内容はMapObjectに一切依存しておらず
//	「std::vector<ComponentEntry>を編集する」という一般的な機能だったため分離した。
//	Player/EnemyエディタなどComponentEntryの配列を持つ他のエディタからも、
//	以下2つのコールバックを渡すだけで同じUIを再利用できる。
//
//	・requestUndoCheckpoint : 値の変更が始まる直前に呼ばれる。
//	                          呼び出し元は自前のUndoスタックへここでPushする
//	・onComponentEdited     : コンポーネントの追加/削除/パラメータ編集が起きるたびに、
//	                          対象のtype名を添えて呼ばれる。呼び出し元は必要な種類
//	                          (例:"ModelRender")だけを見て、プレビュー再構築等の
//	                          副作用を行う
//
//	ComponentInspector自身はUndoの実体もプレビュー用リソースも一切知らない
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
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