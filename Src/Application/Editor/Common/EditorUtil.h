#pragma once

#include <functional>
#include <string>
#include <vector>

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

	std::wstring Utf8ToWide(const std::string& utf8);
	std::string  WideToUtf8(const std::wstring& wide);
}
