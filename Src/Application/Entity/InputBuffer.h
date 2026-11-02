// InputBuffer.h
#pragma once
#include <vector>

// 先行入力を一定時間だけ保持する素のバッファ(Component不要)。
// 同じコマンドは1件に集約し、後から来た入力(猶予・方向)で置き換える。
template <typename TCommand>
class InputBuffer
{
public:
	void Push(TCommand command, const Math::Vector3& direction, float window)
	{
		for (Entry& e : entries_) {
			if (e.command == command) {
				e.timeRemaining = window;
				e.direction = direction;
				return;
			}
		}
		entries_.push_back({ command, window, direction });
	}

	bool Has(TCommand command) const { return Find(command) != entries_.end(); }

	// 消費せずに方向だけ取り出す(実行できた時だけConsumeするため)。
	bool Peek(TCommand command, Math::Vector3* outDirection = nullptr) const
	{
		auto it = Find(command);
		if (it == entries_.end()) return false;
		if (outDirection != nullptr) *outDirection = it->direction;
		return true;
	}

	bool Consume(TCommand command, Math::Vector3* outDirection = nullptr)
	{
		auto it = Find(command);
		if (it == entries_.end()) return false;
		if (outDirection != nullptr) *outDirection = it->direction;
		entries_.erase(it);
		return true;
	}

	void Clear(TCommand command) { Consume(command); }
	void ClearAll() { entries_.clear(); }

	// フレーム頭で呼ぶ。今フレームに積んだ分はその場で減算されない。
	void Tick(float deltaTime)
	{
		for (auto it = entries_.begin(); it != entries_.end();) {
			it->timeRemaining -= deltaTime;
			it = (it->timeRemaining <= 0.0f) ? entries_.erase(it) : it + 1;
		}
	}

private:
	struct Entry
	{
		TCommand command;
		float timeRemaining;
		Math::Vector3 direction;
	};

	typename std::vector<Entry>::const_iterator Find(TCommand command) const
	{
		for (auto it = entries_.begin(); it != entries_.end(); ++it) {
			if (it->command == command) return it;
		}
		return entries_.end();
	}
	typename std::vector<Entry>::iterator Find(TCommand command)
	{
		for (auto it = entries_.begin(); it != entries_.end(); ++it) {
			if (it->command == command) return it;
		}
		return entries_.end();
	}

	std::vector<Entry> entries_;
};
