#pragma once
#include <memory>
#include <vector>
#include "IBTNode.h"

// ============================================================
// Running中の子を最後まで固定して進めるSelector(攻撃のコミット用)。
// 子がRunningを返したら、その子がRunning以外を返すまで他の子は評価しない。
// 外側のreactive Selectorに中断された時はReset()で全子をリセットする。
// ============================================================
template <typename T>
class BTCommittedSelector : public IBTNode<T>
{
public:
	void AddChild(std::unique_ptr<IBTNode<T>> child) { children_.push_back(std::move(child)); }

	BTNodeStatus Tick(T* context, float deltaTime) override
	{
		// 実行中の子がいれば、その子だけを進める
		if (runningIndex_ >= 0) {
			const BTNodeStatus status = children_[runningIndex_]->Tick(context, deltaTime);
			if (status != BTNodeStatus::Running) runningIndex_ = -1;
			return status;
		}

		// 未実行なら先頭から評価し、最初にFailure以外を返した子を採用する
		for (size_t i = 0; i < children_.size(); ++i) {
			const BTNodeStatus status = children_[i]->Tick(context, deltaTime);
			if (status == BTNodeStatus::Failure) continue;
			if (status == BTNodeStatus::Running) runningIndex_ = static_cast<int>(i);
			return status;
		}
		return BTNodeStatus::Failure;
	}

	void Reset() override
	{
		for (auto& child : children_) child->Reset();
		runningIndex_ = -1;
	}

private:
	std::vector<std::unique_ptr<IBTNode<T>>> children_;
	int runningIndex_ = -1; // Running中の子(無ければ-1)
};
