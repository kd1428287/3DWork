#pragma once

#include <functional>
#include <string>
#include <vector>
#include "nlohmann/json.hpp"

// 複数エディタで共有する小さなユーティリティ
namespace EditorUtil
{
	// baseが使用済みなら base2, base3... と連番を付けて、未使用の名前を返す
	std::string MakeUniqueName(const std::string& base, const std::function<bool(const std::string&)>& exists);

	// root以下の画像(png/dds/jpg/jpeg/tga)を再帰走査し、rootからの相対パス(/区切り)を昇順で返す
	std::vector<std::string> ScanTextureFiles(const std::string& root);

	// ファイル選択ダイアログを開き、選んだファイルをrootDirからの相対パス(UTF-8)で返す。
	// filterはOPENFILENAMEのlpstrFilter形式(\0区切り、末尾\0\0)。キャンセル時はfalse
	bool PickFileRelative(const wchar_t* filter, const std::string& rootDir, std::string& outRelative);

	// ModelRenderコンポーネントのparamsが持つモデル参照(実体は"skeleton"の中)。
	//	読み書きはこの2関数に集約し、スキーマが変わってもここだけ直せばよいようにする
	struct ModelRef
	{
		std::string	model;
		std::string	animations;
	};

	template<class Json>
	ModelRef ReadModelRef(const Json& params)
	{
		ModelRef ref;
		if (!params.is_object() || !params.contains("skeleton") || !params["skeleton"].is_object()) { return ref; }

		const auto& skeleton = params["skeleton"];
		ref.model = skeleton.value("model", std::string());
		ref.animations = skeleton.value("animations", std::string());
		return ref;
	}

	template<class Json>
	void WriteModelPath(Json& params, const std::string& path)
	{
		if (!params.is_object()) { params = Json::object(); }
		params["skeleton"]["model"] = path;
	}

	std::wstring Utf8ToWide(const std::string& utf8);
	std::string  WideToUtf8(const std::wstring& wide);
}
