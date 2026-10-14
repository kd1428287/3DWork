#pragma once

// ※ DirectXTK(SimpleMath) は既存のPCH等で読み込まれている前提です。
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "nlohmann/json.hpp"
#include "Application/Definitions/Prefab/Prefab.h"	// ComponentEntry

using UIId = uint32_t;
constexpr UIId kInvalidUIId = 0;

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// UI1要素分のデータ。ゲーム側のGameObject1つに対応し、コンポーネント構成(type + params)で表す。
//	親子は整理用で、座標はUITransformが画面基準で決める(親の位置は影響しない)。
//	兄弟間の並び順 = 描画順(後ろほど手前)。visibleはエディタ上の表示切り替え専用
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct UIElement
{
	UIId		id = kInvalidUIId;
	UIId		parentId = kInvalidUIId;	// 0なら最上位
	std::string	name;
	bool		visible = true;

	std::vector<ComponentEntry>	components;

	ComponentEntry* Find(const std::string& type)
	{
		for (auto& c : components) { if (c.type == type) return &c; }
		return nullptr;
	}
	const ComponentEntry* Find(const std::string& type) const
	{
		for (auto& c : components) { if (c.type == type) return &c; }
		return nullptr;
	}
};

// 1画面分(HUD・タイトル・メニューなど)。画面ごとに1つのJSONファイルにする
struct UIFile
{
	DirectX::SimpleMath::Vector2	refSize = { 1920.0f, 1080.0f };	// 基準解像度(実行時のビューポート想定)
	UIId							nextId = 1;
	std::vector<UIElement>			elements;
};

namespace UIDataDetail
{
	inline DirectX::SimpleMath::Vector2 ReadVec2(const nlohmann::json& j, const char* key,
		const DirectX::SimpleMath::Vector2& def)
	{
		if (j.contains(key) && j[key].is_array() && j[key].size() >= 2
			&& j[key][0].is_number() && j[key][1].is_number())
		{
			return DirectX::SimpleMath::Vector2(j[key][0].get<float>(), j[key][1].get<float>());
		}
		return def;
	}

	inline DirectX::SimpleMath::Vector4 ReadVec4(const nlohmann::json& j, const char* key,
		const DirectX::SimpleMath::Vector4& def)
	{
		if (j.contains(key) && j[key].is_array() && j[key].size() >= 4)
		{
			return DirectX::SimpleMath::Vector4(
				j[key][0].get<float>(), j[key][1].get<float>(),
				j[key][2].get<float>(), j[key][3].get<float>());
		}
		return def;
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// UITransformの設定と矩形の解決
//	実行時のUITransformComponentと同じ規則(画面基準のanchor・中心原点Y上向き)で解決する。
//	矩形(UIRect)だけは、エディタ表示用に左上原点・Y下向きで返す
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct UILayout
{
	bool							world = false;	// Space::World(投影が必要なのでエディタでは表示しない)
	DirectX::SimpleMath::Vector2	anchor = { 0.5f, 0.5f };
	DirectX::SimpleMath::Vector2	pivot = { 0.5f, 0.5f };
	DirectX::SimpleMath::Vector2	position = { 0.0f, 0.0f };
	DirectX::SimpleMath::Vector2	size = { 0.0f, 0.0f };
	DirectX::SimpleMath::Vector2	scale = { 1.0f, 1.0f };
};

struct UIRect
{
	DirectX::SimpleMath::Vector2	min;	// 左上
	DirectX::SimpleMath::Vector2	size;
};

// UITransformコンポーネントが無ければfalse
inline bool ReadUILayout(const UIElement& e, UILayout& out)
{
	const ComponentEntry* c = e.Find("UITransform");
	if (!c || !c->params.is_object()) return false;

	const nlohmann::json& p = c->params;
	out.world = p.value("space", std::string("Screen")) == "World";
	out.anchor = UIDataDetail::ReadVec2(p, "anchor", out.anchor);
	out.pivot = UIDataDetail::ReadVec2(p, "pivot", out.pivot);
	out.position = UIDataDetail::ReadVec2(p, "position", out.position);
	out.size = UIDataDetail::ReadVec2(p, "size", out.size);
	out.scale = UIDataDetail::ReadVec2(p, "scale", out.scale);
	return true;
}

// positionとsizeだけを書き戻す(他のキーは触らない)
inline void WriteUILayout(UIElement& e, const UILayout& l)
{
	ComponentEntry* c = e.Find("UITransform");
	if (!c) return;
	if (!c->params.is_object()) c->params = nlohmann::json::object();

	c->params["position"] = { l.position.x, l.position.y };
	c->params["size"] = { l.size.x, l.size.y };
}

// 基準点(pivotの位置)。中心原点・Y上向きで、UITransformComponent::Resolve()と同じ
inline DirectX::SimpleMath::Vector2 ResolveUIBase(const UILayout& l, const DirectX::SimpleMath::Vector2& refSize)
{
	return DirectX::SimpleMath::Vector2(
		(l.anchor.x - 0.5f) * refSize.x + l.position.x,
		(l.anchor.y - 0.5f) * refSize.y + l.position.y);
}

// 大きさsizeの矩形を、pivotと位置に従って置く(エディタ座標で返す)
inline UIRect ResolveUIBox(const UILayout& l, const DirectX::SimpleMath::Vector2& refSize,
	const DirectX::SimpleMath::Vector2& size)
{
	const DirectX::SimpleMath::Vector2 base = ResolveUIBase(l, refSize);
	const float leftUp = base.x - size.x * l.pivot.x;
	const float bottomUp = base.y - size.y * l.pivot.y;

	UIRect r;
	r.size = size;
	r.min = DirectX::SimpleMath::Vector2(leftUp + refSize.x * 0.5f, refSize.y * 0.5f - (bottomUp + size.y));
	return r;
}

// 矩形(エディタ座標)に合うよう、UITransformのposition(resize時はsizeも)を逆算して書き込む
inline void ApplyUIRect(UIElement& e, const DirectX::SimpleMath::Vector2& refSize, const UIRect& rect, bool resize)
{
	UILayout l;
	if (!ReadUILayout(e, l)) return;

	const float leftUp = rect.min.x - refSize.x * 0.5f;
	const float bottomUp = refSize.y * 0.5f - (rect.min.y + rect.size.y);
	const float baseX = leftUp + rect.size.x * l.pivot.x;
	const float baseY = bottomUp + rect.size.y * l.pivot.y;

	l.position = DirectX::SimpleMath::Vector2(
		baseX - (l.anchor.x - 0.5f) * refSize.x,
		baseY - (l.anchor.y - 0.5f) * refSize.y);

	// sizeは拡大率適用前の値で持つ
	if (resize)
	{
		l.size = DirectX::SimpleMath::Vector2(
			rect.size.x / (l.scale.x != 0.0f ? l.scale.x : 1.0f),
			rect.size.y / (l.scale.y != 0.0f ? l.scale.y : 1.0f));
	}

	WriteUILayout(e, l);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// JSON入出力(実装はUIData.cpp。ファイルI/Oとスキーマ変換はここに一元化する)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

// pathからUIを読み込む。ファイルが無い/JSONとして壊れている場合はfalseを返す(outは変更しない)。
// 壊れた要素はスキップして続行し、idの欠落・重複や不正な親は読み込み時に直す
bool LoadUIFile(const std::string& path, UIFile& out);

// pathへ保存する(常に現行バージョンの形式で書き出す)
bool SaveUIFile(const std::string& path, const UIFile& file);

// 保存するJSONそのもの。未保存の変更の検出(保存済みとの比較)にも使う
nlohmann::json UIFileToJson(const UIFile& file);

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 画面マニフェスト(画面名 → UIのJSONパス)
//	ゲーム側は画面名で呼び出し、エディタはこの一覧から編集する画面を選ぶ。
//	並び順を保つため、画面は連想配列でなく配列で持つ(エディタのコンボの表示順になる)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct UIScreenEntry
{
	std::string	name;	// 英数字・_・-
	std::string	path;
};

struct UIManifest
{
	std::vector<UIScreenEntry>	screens;

	// 名前が一致する画面の添字(無ければ-1)
	int Find(const std::string& name) const
	{
		for (size_t i = 0; i < screens.size(); ++i) { if (screens[i].name == name) return (int)i; }
		return -1;
	}
};

// 読めない・形式が違う場合はfalse。名前かパスが空の項目と、名前が重複する項目は読み飛ばす
bool LoadUIManifest(const std::string& path, UIManifest& out);
bool SaveUIManifest(const std::string& path, const UIManifest& manifest);