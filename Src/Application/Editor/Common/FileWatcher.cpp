#include "Application/main.h"

#include "FileWatcher.h"

bool FileWatcher::Poll(const std::string& path, float deltaTime)
{
	if (!autoReload) { return false; }

	timer_ += deltaTime;
	if (timer_ < interval) { return false; }
	timer_ = 0.0f;

	FILETIME writeTime;
	if (!JsonLoader::GetLastWriteTime(path, writeTime)) { return false; }

	// 初回は基準値を覚えるだけ
	if (lastWriteTime_.dwLowDateTime == 0 && lastWriteTime_.dwHighDateTime == 0)
	{
		lastWriteTime_ = writeTime;
		return false;
	}

	if (CompareFileTime(&writeTime, &lastWriteTime_) == 0) { return false; }

	// 読込失敗で毎回リトライしてログが溢れないよう、先に更新しておく
	lastWriteTime_ = writeTime;
	return true;
}

void FileWatcher::Sync(const std::string& path)
{
	FILETIME writeTime;
	if (JsonLoader::GetLastWriteTime(path, writeTime)) { lastWriteTime_ = writeTime; }
}
