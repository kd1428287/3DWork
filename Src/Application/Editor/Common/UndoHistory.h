#pragma once

#include <vector>

// スナップショット方式のUndo/Redo履歴。Tは「適用に必要な状態一式」(各エディタのUndoState等)
template<class T>
class UndoHistory
{
public:
	// 変更を加える直前の状態を積む(Redoは破棄される)
	void Push(const T& before)
	{
		undo_.push_back(before);
		if (undo_.size() > kMaxDepth) { undo_.erase(undo_.begin()); }
		redo_.clear();
	}

	// currentを1つ前の状態に差し替える(元のcurrentはRedoへ)。差し替えたらtrue
	bool Undo(T& current) { return Swap(undo_, redo_, current); }
	bool Redo(T& current) { return Swap(redo_, undo_, current); }

	bool CanUndo() const { return !undo_.empty(); }
	bool CanRedo() const { return !redo_.empty(); }
	void Clear() { undo_.clear(); redo_.clear(); }

private:
	static bool Swap(std::vector<T>& from, std::vector<T>& to, T& current)
	{
		if (from.empty()) { return false; }
		to.push_back(std::move(current));
		current = std::move(from.back());
		from.pop_back();
		return true;
	}

	static constexpr size_t kMaxDepth = 50;

	std::vector<T> undo_;
	std::vector<T> redo_;
};
