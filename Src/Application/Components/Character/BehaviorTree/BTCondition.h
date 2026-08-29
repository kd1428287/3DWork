#pragma once
#include <functional>
#include "IBTNode.h"

// ============================================================
// 「条件を満たすかどうかだけを判定する」汎用の葉ノード。
// BTSequence/BTSelectorの各分岐で「この分岐に進んでよいか」を
// チェックする役割(例: 射程内か？発見中か？)。
//
// Condition専用の派生クラスを条件の数だけ作ると冗長になるため、
// 述語(ラムダ/関数ポインタ)を1つ受け取るだけの汎用ノードにしている。
// 常にSuccess/Failureのどちらかしか返さない(Runningは返さない
// = 判定は必ず1フレームで完結する、という契約)。
// ============================================================
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
