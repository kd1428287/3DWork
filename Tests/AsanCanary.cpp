#include "MiniTest.h"

#ifndef TEST_CASE
#error TEST_CASE is not defined
#endif

TEST_CASE("sanity") { CHECK(1 + 1 == 2); }

TEST_CASE_SKIPPED("asan canary") {
	int* p = new int(1);
	delete p;
	volatile int x = *p;   // 解放済みを読む
	(void)x;
}