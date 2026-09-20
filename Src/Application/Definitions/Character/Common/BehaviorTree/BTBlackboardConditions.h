#pragma once
#include "IBTNode.h"
#include "Blackboard.h"

// ============================================================
// Blackboardの値を条件判定に使う、汎用のCondition葉ノード群。
//
// 【BTCondition<T>との違い】
// BTCondition<T>はC++のラムダ(std::function<bool(T*)>)を直接持つため、
// 条件の中身はBuildTree()を書くその場でC++コードとして書く必要がある。
// これは手書きのC++ツリーでは問題にならないが、ノードグラフエディタで
// ツリーを組む場合、ノード自身がC++の型やメンバ関数を一切知らない
// 「キー名+比較条件」だけのデータとして表現できる必要がある。
// この2つのクラスは、それを実現するための最小限の語彙(bool一致・
// float比較)を提供する。BTCondition<T>自体は、C++側だけで完結する
// 内部ロジック向けに引き続き使ってよい(エディタが生成するツリーの
// 中では使えない、というだけ)。
//
// 【キーが未設定(または型違い)の場合の扱い】
// 「条件を判定できない」状態とみなし、常にFailureを返す
// (BTCompareBoolConditionでexpected=falseを指定していても、キー自体が
// 一度も設定されていなければFailureにする。「未設定」と「falseが
// 設定されている」を混同しないため)。ノードグラフでキー名を打ち間違えた
// 場合、常にこの分岐が選ばれなくなるだけで安全側に倒れる
// (Blackboard::SetBool/SetFloat時点の型不一致診断については
// Blackboard.h側の【要検討】コメント参照。両者は別の問題)。
//
// Tには継承を要求しない。以下のメンバ関数を持っていればよい:
//   Blackboard& GetBlackboard() / const Blackboard& GetBlackboard() const
// ============================================================

// float比較の演算子。BTCompareFloatConditionが使う。
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
