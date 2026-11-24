// doctest互換の最小サブセット。mainを持たせたい1つのcppで MINITEST_IMPLEMENT_MAIN を定義してincludeする。
#pragma once
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace minitest {

	struct TestCase {
		const char* name;
		void (*func)();
		bool skipByDefault;
	};

	struct RequireFailed {};

	inline std::vector<TestCase>& Tests() { static std::vector<TestCase> t; return t; }
	inline int& FailCount() { static int n = 0; return n; }

	struct Registrar {
		Registrar(const char* name, void (*func)(), bool skip) { Tests().push_back({ name, func, skip }); }
	};

	inline void Report(bool ok, const char* expr, const char* file, int line) {
		if (ok) return;
		++FailCount();
		std::fprintf(stderr, "%s(%d): FAILED: %s\n", file, line, expr);
	}

}  // namespace minitest

#define MT_CAT2(a, b) a##b
#define MT_CAT(a, b) MT_CAT2(a, b)

// 通常のテスト
#define TEST_CASE(name) \
	static void MT_CAT(mt_fn_, __LINE__)(); \
	static minitest::Registrar MT_CAT(mt_reg_, __LINE__)(name, &MT_CAT(mt_fn_, __LINE__), false); \
	static void MT_CAT(mt_fn_, __LINE__)()

// --no-skip を付けた時だけ走るテスト(ASanのカナリア用)
#define TEST_CASE_SKIPPED(name) \
	static void MT_CAT(mt_fn_, __LINE__)(); \
	static minitest::Registrar MT_CAT(mt_reg_, __LINE__)(name, &MT_CAT(mt_fn_, __LINE__), true); \
	static void MT_CAT(mt_fn_, __LINE__)()

#define CHECK(expr)       minitest::Report(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define CHECK_FALSE(expr) minitest::Report(!static_cast<bool>(expr), "!(" #expr ")", __FILE__, __LINE__)
#define CHECK_EQ(a, b)    minitest::Report((a) == (b), #a " == " #b, __FILE__, __LINE__)
#define CHECK_NE(a, b)    minitest::Report((a) != (b), #a " != " #b, __FILE__, __LINE__)

// 失敗したらそのテストを中断する
#define REQUIRE(expr) \
	do { const bool mt_ok = static_cast<bool>(expr); \
		minitest::Report(mt_ok, #expr, __FILE__, __LINE__); \
		if (!mt_ok) throw minitest::RequireFailed{}; } while (0)

#ifdef MINITEST_IMPLEMENT_MAIN
// 使い方: Tests.exe [-tc=部分一致の名前] [--no-skip]
int main(int argc, char** argv) {
	std::string filter;
	bool noSkip = false;
	for (int i = 1; i < argc; ++i) {
		if (std::strncmp(argv[i], "-tc=", 4) == 0) filter = argv[i] + 4;
		else if (std::strcmp(argv[i], "--no-skip") == 0) noSkip = true;
	}
	filter.erase(std::remove(filter.begin(), filter.end(), '*'), filter.end());

	int ran = 0, failedTests = 0;
	for (const auto& t : minitest::Tests()) {
		if (t.skipByDefault && !noSkip) continue;
		if (!filter.empty() && std::string(t.name).find(filter) == std::string::npos) continue;

		const int before = minitest::FailCount();
		try { t.func(); }
		catch (const minitest::RequireFailed&) {}
		++ran;
		if (minitest::FailCount() != before) {
			++failedTests;
			std::fprintf(stderr, "  in test case: %s\n", t.name);
		}
	}
	std::printf("[MiniTest] %d test cases run, %d failed\n", ran, failedTests);
	return failedTests == 0 ? 0 : 1;
}
#endif