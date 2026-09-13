#pragma once
#include "ComponentTypes.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// マップ上の1エンティティを表す、唯一の正となるスキーマ。
//	MapEditor(編集時)・MapLoader/TerrainFactory(実行時)の両方が直接この構造体を使う。
//	以前はMapObject(Editor)とEntityData(Runtime)という2つの構造体を手動で同期させていたが、
//	フィールドを増やすたびに複数箇所を揃える必要があり、実際にスキーマの食い違いによる
//	不具合を起こした。1つの構造体に統一することでこれを根本から無くす。
//
//	回転はQuaternionで一意に持つ(Euler角は保存しない)。ImGuizmo操作やInspector表示など
//	「人間がEuler角で見たい/操作したい」場面は、その場でQuaternion⇔Eulerの変換を行うだけに
//	留め、変換結果を保存・往復させることはしない(合成順序の不一致によるズレを構造的に防ぐ)。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
using ObjectId = uint64_t;
constexpr ObjectId kInvalidObjectId = 0;

struct MapEntity
{
	ObjectId		id = kInvalidObjectId;	// マップ内で一意。保存され、Undo/Redoや将来の
	// オブジェクト参照(トリガー等)の安定した拠り所になる
	std::string		name = "Object";

	Math::Vector3		pos = { 0.0f, 0.0f, 0.0f };
	Math::Quaternion	rotation = Math::Quaternion::Identity;
	Math::Vector3		scale = { 1.0f, 1.0f, 1.0f };

	std::vector<ComponentEntry>	components;
};

// 読み込んだマップ全体。nextIdは「これから新規追加するオブジェクトに使うべきID」を、
// 読み込んだ内容から計算済みの状態で渡す(呼び出し側が自前でmaxを取り直さなくてよいように)
struct MapFile
{
	std::vector<MapEntity>	entities;
	ObjectId				nextId = 1;
};

// path からマップを読み込む。ファイルが無い/JSONとして壊れている場合はfalseを返す(outは変更しない)。
// 個々のエンティティのフィールド欠落・型不一致は、そのエンティティだけをスキップして続行する
// (1件の不正データでマップ全体の読み込みを失敗させない)。
// 旧バージョンのファイル形式(ルート直下が配列、rotateがEuler角、componentsが無く"model"直書き等)
// も自動的に現行バージョンへ変換してから読み込む(下記MigrateToLatest参照)。
bool LoadMapFile(const std::string& path, MapFile& out);

// path へマップを保存する(常に現行バージョンの形式で書き出す)
bool SaveMapFile(const std::string& path, const std::vector<MapEntity>& entities);