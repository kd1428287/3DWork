#pragma once

#include <string>
#include <windows.h>

// JSONの更新日時をポーリングし、外部変更を検知する(ホットリロード用)
class FileWatcher
{
public:
	// 前回から更新されていればtrue。初回観測は基準値を覚えるだけでfalse
	bool Poll(const std::string& path, float deltaTime);

	// 自分で保存/読込した直後に呼ぶ(自分の書き込みを外部変更と誤検知しない)
	void Sync(const std::string& path);

	// 監視対象が変わった時などに基準値を捨てる
	void Reset() { lastWriteTime_ = {}; timer_ = 0.0f; }

	bool autoReload = true;		// UIのチェックボックスへ直接バインドする
	float interval = 0.5f;		// ポーリング間隔(秒)

private:
	float		timer_ = 0.0f;
	FILETIME	lastWriteTime_ = {};
};
