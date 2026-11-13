#pragma once
#include "IBTNode.h"

// 述語(ラムダ)で条件を判定する汎用の葉ノード。Runningは返さず、1フレームで完結する。
template <typename T>
class BTCondition : public IBTNode<T>
{
public:
	using Predicate = std::function<bool(T*)>;

	explicit BTCondition(Predicate predicate) : predicate_(std::move(predicate)) {}

	BTNodeStatus Tick(T* context, float /*deltaTime*/) override
	{
		return predicate_(context) ? BTNodeStatus::Success : BTNodeStatus::Failure;
	}

private:
	Predicate predicate_;
};
