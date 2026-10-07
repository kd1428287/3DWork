#pragma once

// ※ DirectXTK(SimpleMath) は既存のPCH等で読み込まれている前提です。
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "nlohmann/json.hpp"

using UIId = uint32_t;
constexpr UIId kInvalidUIId = 0;

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// UI1要素分のデータ。保存・実行時に使うスキーマはこれが唯一の正。
//	type: "Image" / "Text" / "Button"。種別ごとの固有値はparamsに持つ
//	  Image : texture
//	  Text  : text, fontSize
//	  Button: text, fontSize, action(押下時に発行するイベント名)
//	兄弟間の並び順 = 描画順(後ろほど手前) = メニューのフォーカス順
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct UIElement
{
	UIId		id = kInvalidUIId;
	UIId		parentId = kInvalidUIId;	// 0なら画面直下
	std::string	name;
	std::string	type = "Image";

	DirectX::SimpleMath::Vector2	anchor = { 0.0f, 0.0f };	// 親矩形内の基準位置(0〜1)
	DirectX::SimpleMath::Vector2	pivot = { 0.0f, 0.0f };		// 自身の矩形内の基準点(0〜1)
	DirectX::SimpleMath::Vector2	pos = { 0.0f, 0.0f };		// アンカー位置からのオフセット(px)
	DirectX::SimpleMath::Vector2	size = { 100.0f, 100.0f };
	DirectX::SimpleMath::Vector4	color = { 1.0f, 1.0f, 1.0f, 1.0f };
	bool							visible = true;

	nlohmann::json	params = nlohmann::json::object();

	bool operator==(const UIElement& o) const
	{
		return id == o.id && parentId == o.parentId && name == o.name && type == o.type
			&& anchor == o.anchor && pivot == o.pivot && pos == o.pos && size == o.size
			&& color == o.color && visible == o.visible && params == o.params;
	}
};

// 1画面分(HUD・タイトル・メニューなど)。画面ごとに1つのJSONファイルにする
struct UIFile
{
	DirectX::SimpleMath::Vector2	refSize = { 1920.0f, 1080.0f };	// 基準解像度
	UIId							nextId = 1;
	std::vector<UIElement>			elements;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 矩形の解決(基準解像度上のピクセル座標)。エディタと実行時で共通に使う
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct UIRect
{
	DirectX::SimpleMath::Vector2	min;
	DirectX::SimpleMath::Vector2	size;
};

inline UIRect ResolveUIRect(const std::vector<UIElement>& all, const UIElement& e,
	const DirectX::SimpleMath::Vector2& refSize, int depth);

// 親の矩形(親が無ければ画面全体)
inline UIRect ResolveParentRect(const std::vector<UIElement>& all, const UIElement& e,
	const DirectX::SimpleMath::Vector2& refSize, int depth = 0)
{
	UIRect r;
	r.min = DirectX::SimpleMath::Vector2(0.0f, 0.0f);
	r.size = refSize;

	if (e.parentId == kInvalidUIId || depth >= 32) return r;

	for (auto& p : all)
	{
		if (p.id == e.parentId) return ResolveUIRect(all, p, refSize, depth + 1);
	}
	return r;
}

inline UIRect ResolveUIRect(const std::vector<UIElement>& all, const UIElement& e,
	const DirectX::SimpleMath::Vector2& refSize, int depth = 0)
{
	const UIRect parent = ResolveParentRect(all, e, refSize, depth);

	UIRect r;
	r.size = e.size;
	r.min = parent.min + parent.size * e.anchor + e.pos - e.size * e.pivot;
	return r;
}

// 見た目の矩形(rect)になるように、eのpos/sizeを逆算して書き込む
inline void ApplyUIRect(UIElement& e, const UIRect& parentRect, const UIRect& rect)
{
	e.size = rect.size;
	e.pos = rect.min - parentRect.min - parentRect.size * e.anchor + rect.size * e.pivot;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// JSON入出力
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
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

inline bool SaveUIFile(const std::string& path, const UIFile& file)
{
	using json = nlohmann::json;

	json root;
	root["version"] = 1;
	root["refSize"] = { file.refSize.x, file.refSize.y };
	root["nextId"] = file.nextId;

	json arr = json::array();
	for (auto& e : file.elements)
	{
		json j;
		j["id"] = e.id;
		j["parent"] = e.parentId;
		j["name"] = e.name;
		j["type"] = e.type;
		j["anchor"] = { e.anchor.x, e.anchor.y };
		j["pivot"] = { e.pivot.x, e.pivot.y };
		j["pos"] = { e.pos.x, e.pos.y };
		j["size"] = { e.size.x, e.size.y };
		j["color"] = { e.color.x, e.color.y, e.color.z, e.color.w };
		j["visible"] = e.visible;
		j["params"] = e.params;
		arr.push_back(std::move(j));
	}
	root["elements"] = std::move(arr);

	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

	std::ofstream ofs(path);
	if (!ofs) return false;

	ofs << root.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
	return true;
}

inline bool LoadUIFile(const std::string& path, UIFile& out)
{
	using json = nlohmann::json;

	std::ifstream ifs(path);
	if (!ifs) return false;

	json root = json::parse(ifs, nullptr, false);
	if (root.is_discarded() || !root.is_object()) return false;

	UIFile file;
	file.refSize = UIDataDetail::ReadVec2(root, "refSize", file.refSize);

	UIId maxId = 0;
	if (root.contains("elements") && root["elements"].is_array())
	{
		for (auto& j : root["elements"])
		{
			UIElement e;
			e.id = j.value("id", 0u);
			if (e.id == kInvalidUIId) continue;

			e.parentId = j.value("parent", 0u);
			e.name = j.value("name", std::string());
			e.type = j.value("type", std::string("Image"));
			e.anchor = UIDataDetail::ReadVec2(j, "anchor", e.anchor);
			e.pivot = UIDataDetail::ReadVec2(j, "pivot", e.pivot);
			e.pos = UIDataDetail::ReadVec2(j, "pos", e.pos);
			e.size = UIDataDetail::ReadVec2(j, "size", e.size);
			e.color = UIDataDetail::ReadVec4(j, "color", e.color);
			e.visible = j.value("visible", true);
			if (j.contains("params") && j["params"].is_object()) e.params = j["params"];

			if (e.id > maxId) maxId = e.id;
			file.elements.push_back(std::move(e));
		}
	}

	file.nextId = root.value("nextId", 1u);
	if (file.nextId <= maxId) file.nextId = maxId + 1;

	out = std::move(file);
	return true;
}