#pragma once
#include "IBTNode.h"
#include "Blackboard.h"

// Blackboardの値を判定する汎用Condition。ラムダを持たず「キー名+比較条件」だけで表せるため、
// ノードグラフエディタが組むツリーでも使える(BTCondition<T>はC++専用)。
// キーが未設定(または型違い)の場合は常にFailure。「未設定」と「falseが設定済み」は区別する。
// Tに継承は不要で、GetBlackboard()(const版も)を持っていればよい。

// BTCompareFloatConditionの比較演算子
enum class CompareOp
{
	Less,
	LessEqual,
	Greater,
	GreaterEqual,
	Equal,
	NotEqual,
};

template <typename T>
class BTCompareBoolCondition : public IBTNode<T>
{
public:
	BTCompareBoolCondition(std::string key, bool expected = true)
		: key_(std::move(key)), expected_(expected) {
	}

	BTNodeStatus Tick(T* context, float /*deltaTime*/) override
	{
		auto value = context->GetBlackboard().GetBool(key_);
		if (!value) return BTNodeStatus::Failure;
		return (*value == expected_) ? BTNodeStatus::Success : BTNodeStatus::Failure;
	}

private:
	std::string key_;
	bool expected_;
};

template <typename T>
class BTCompareFloatCondition : public IBTNode<T>
{
public:
	BTCompareFloatCondition(std::string key, CompareOp op, float threshold)
		: key_(std::move(key)), op_(op), threshold_(threshold) {
	}

	BTNodeStatus Tick(T* context, float /*deltaTime*/) override
	{
		auto value = context->GetBlackboard().GetFloat(key_);
		if (!value) return BTNodeStatus::Failure;
		return Compare(*value) ? BTNodeStatus::Success : BTNodeStatus::Failure;
	}

private:
	bool Compare(float value) const
	{
		switch (op_) {
		case CompareOp::Less:         return value < threshold_;
		case CompareOp::LessEqual:    return value <= threshold_;
		case CompareOp::Greater:      return value > threshold_;
		case CompareOp::GreaterEqual: return value >= threshold_;
		case CompareOp::Equal:        return value == threshold_;
		case CompareOp::NotEqual:     return value != threshold_;
		default:                      return false;
		}
	}

	std::string key_;
	CompareOp op_;
	float threshold_;
};
