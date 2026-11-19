#pragma once

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

// KdAssets(KdDataStorage.h)へロードする対象をまとめ、同期で順に読み込む。
// フォルダ指定の一括登録、進捗取得、時間予算つきのStep()に対応(ローディング画面用)。

enum class AssetType
{
	Texture,
	Model,
	Animation,
};

class LoadList
{
public:
	// 単品を登録。同じ種別・同じパス(正規化後)は1件にまとまる
	void AddFile(AssetType type, const std::string& path)
	{
		const std::string norm = NormalizePath(path);
		if (!m_registered.insert(std::to_string((int)type) + "|" + norm).second) { return; }

		m_entries.push_back({ type, norm });
	}

	// dir以下を再帰走査し、拡張子(".gltf"等、小文字)が一致するものを登録。excludeDirsは除外するフォルダ名
	void AddFolder(AssetType type, const std::string& dir, const std::vector<std::string>& exts,
		const std::vector<std::string>& excludeDirs = {})
	{
		namespace fs = std::filesystem;

		std::error_code ec;
		if (!fs::is_directory(dir, ec)) { return; }

		// 並び順を毎回同じにするため、パスを集めてソートしてから登録する
		std::vector<std::string> found;
		for (fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
			it != end; it.increment(ec))
		{
			if (ec) { break; }

			if (it->is_directory(ec))
			{
				if (IsContained(excludeDirs, it->path().filename().string())) { it.disable_recursion_pending(); }
				continue;
			}
			if (!it->is_regular_file(ec)) { continue; }
			if (!IsContained(exts, ToLower(it->path().extension().string()))) { continue; }

			found.push_back(it->path().string());
		}

		std::sort(found.begin(), found.end());
		for (const auto& path : found) { AddFile(type, path); }
	}

	// 全件を一度に読み込む
	void LoadAll()
	{
		while (!IsDone()) { LoadNext(); }
	}

	// 予算(ミリ秒)に達するまで読み込んで戻る。全件完了でtrue。1件ごとに時間を確認するため、1件の途中では止まらない
	bool Step(float budgetMs)
	{
		const auto start = Clock::now();
		while (!IsDone())
		{
			LoadNext();
			if (ToMs(Clock::now() - start) >= budgetMs) { break; }
		}
		return IsDone();
	}

	bool IsDone() const { return m_next >= m_entries.size(); }
	size_t GetTotal() const { return m_entries.size(); }
	size_t GetLoaded() const { return m_next; }
	float GetProgress() const { return m_entries.empty() ? 1.0f : (float)m_next / (float)m_entries.size(); }

	// 次に読み込むファイル名(ローディング画面の表示用)。完了後は空
	const std::string& GetNextPath() const { return IsDone() ? m_empty : m_entries[m_next].path; }

	// 読み込みに失敗したパス
	const std::vector<std::string>& GetFailed() const { return m_failed; }

	// 取得済みの参照を手放す(ClearData(false)で解放可能になる)
	void Release() { m_holders.clear(); }

private:
	using Clock = std::chrono::steady_clock;

	struct Entry
	{
		AssetType type;
		std::string path;
	};

	void LoadNext()
	{
		const Entry& e = m_entries[m_next++];
		const auto start = Clock::now();

		// 結果を保持して、シーン切替時のClearData(false)で解放されないようにする
		std::shared_ptr<void> data;
		switch (e.type)
		{
		case AssetType::Texture:   data = KdAssets::Instance().m_textures.GetData(e.path);   break;
		case AssetType::Model:     data = KdAssets::Instance().m_modeldatas.GetData(e.path); break;
		case AssetType::Animation: data = KdAssets::Instance().m_animations.GetData(e.path); break;
		}

		if (data) { m_holders.push_back(std::move(data)); }
		else { m_failed.push_back(e.path); }

		Log(e.path, ToMs(Clock::now() - start));
	}

	// 相対パス・'/'区切りに揃える(KdDataStorageのキーが文字列一致のため、二重ロード防止)
	static std::string NormalizePath(const std::string& path)
	{
		namespace fs = std::filesystem;

		fs::path p = fs::path(path).lexically_normal();
		std::error_code ec;
		if (p.is_absolute())
		{
			const fs::path rel = p.lexically_relative(fs::current_path(ec));
			if (!rel.empty() && *rel.begin() != "..") { p = rel; }
		}
		return p.generic_string();
	}

	static std::string ToLower(std::string s)
	{
		for (auto& c : s) { c = (char)std::tolower((unsigned char)c); }
		return s;
	}

	static bool IsContained(const std::vector<std::string>& list, const std::string& s)
	{
		return std::find(list.begin(), list.end(), s) != list.end();
	}

	static float ToMs(Clock::duration d) { return std::chrono::duration<float, std::milli>(d).count(); }

	// ロード時間の計測ログ(遅い物の特定用)
	static void Log(const std::string& path, float ms)
	{
#ifdef _WIN32
		OutputDebugStringA(("[LoadList] " + std::to_string((int)ms) + "ms : " + path + "\n").c_str());
#endif
	}

	std::vector<Entry> m_entries;
	std::unordered_set<std::string> m_registered;
	size_t m_next = 0;

	std::vector<std::shared_ptr<void>> m_holders;
	std::vector<std::string> m_failed;
	std::string m_empty;
};
