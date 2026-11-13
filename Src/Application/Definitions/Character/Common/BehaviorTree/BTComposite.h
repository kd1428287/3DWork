#pragma once
#include "IBTNode.h"

// 子を上から順に実行し、Failureが出たら即Failure、全員SuccessならSuccess(AND相当)。
// 前回Runningだった子から再開する。
template <typename T>
class BTSequence : public IBTNode<T>
{
public:
	void AddChild(std::unique_ptr<IBTNode<T>> child) { children_.push_back(std::move(child)); }

	BTNodeStatus Tick(T* context, float deltaTime) override
	{
		while (runningIndex_ < children_.size()) {
			const BTNodeStatus status = children_[runningIndex_]->Tick(context, deltaTime);
			if (status == BTNodeStatus::Running) return BTNodeStatus::Running;
			if (status == BTNodeStatus::Failure) {
				runningIndex_ = 0;
				return BTNodeStatus::Failure;
			}
			++runningIndex_;	// Successなら次の子へ
		}
		runningIndex_ = 0;
		return BTNodeStatus::Success;
	}

	void Reset() override
	{
		if (runningIndex_ < children_.size()) children_[runningIndex_]->Reset();
		runningIndex_ = 0;
	}

private:
	std::vector<std::unique_ptr<IBTNode<T>>> children_;
	size_t runningIndex_ = 0;
};

// 子を上から順に試し、最初にSuccess/Runningを返した子を採用する(全員FailureならFailure)。
// 毎フレーム先頭から評価し直す(reactive方式)ため、高優先の行動が可能になればすぐ割り込める。
// 代償としてCondition評価が毎フレーム走るので、Conditionは軽い処理にすること。
template <typename T>
class BTSelector : public IBTNode<T>
{
public:
	void AddChild(std::unique_ptr<IBTNode<T>> child) { children_.push_back(std::move(child)); }

	BTNodeStatus Tick(T* context, float deltaTime) override
	{
		for (size_t i = 0; i < children_.size(); ++i) {
			const BTNodeStatus status = children_[i]->Tick(context, deltaTime);

			if (status == BTNodeStatus::Running || status == BTNodeStatus::Success) {
				// 別の子がRunningだった場合は、割り込まれたものとしてResetする
				if (hasRunning_ && runningIndex_ != i) {
					children_[runningIndex_]->Reset();
				}
				hasRunning_ = (status == BTNodeStatus::Running);
				runningIndex_ = i;
				return status;
			}
			// Failureなら同フレーム内で次の子を試す
		}

		// 全員Failure。Running中だった子も実行不能になったとみなしてResetする
		if (hasRunning_) {
			children_[runningIndex_]->Reset();
			hasRunning_ = false;
		}
		return BTNodeStatus::Failure;
	}

	void Reset() override
	{
		if (hasRunning_) {
			children_[runningIndex_]->Reset();
			hasRunning_ = false;
		}
	}

private:
	std::vector<std::unique_ptr<IBTNode<T>>> children_;
	size_t runningIndex_ = 0;
	// runningIndex_==0だけでは「0番がRunning中」と「初期状態」を区別できないため別に持つ
	bool hasRunning_ = false;
};
