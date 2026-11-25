#pragma once

#include <functional>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

// 指定フォルダ以下(サブフォルダ含む)のJSONから、条件に合うものを一覧表示して選ばせる
class JsonFileList
{
public:

	using Filter = std::function<bool(const nlohmann::json&)>;

	JsonFileList(const char* defaultDir, Filter filter);

	// クリックされたらoutPathに入れてtrue。heightが0なら呼び出し元ウィンドウへ直接並べる
	bool Draw(const std::string& currentPath, std::string& outPath, float height = 0.0f);

	void Invalidate() { m_loaded = false; }		// 次の描画で再走査する
	const char* Dir() const { return m_dirBuf; }

private:

	void Refresh();

	struct Entry { std::string label; std::string path; };

	char				m_dirBuf[260] = "";
	Filter				m_filter;
	std::vector<Entry>	m_entries;
	bool				m_loaded = false;
};

namespace EditorFileUI
{
	enum class UnsavedChoice { None, Save, Discard };

	// 未保存変更の確認ダイアログ。毎フレーム呼ぶ
	UnsavedChoice DrawUnsavedPopup(bool& openRequested, bool canSave);
}
