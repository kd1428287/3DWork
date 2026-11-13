#pragma once
#include "BTNodeStatus.h"

// ビヘイビアツリーのノード基底。TはツリーがぶらさがるController型(EnemyAIController等)で、
// ノードはその公開APIを直接呼んで行動を実行する。
template <typename T>
class IBTNode
{
public:
	virtual ~IBTNode() = default;

	// 毎フレーム評価する。Runningを返すと次フレームも評価される。
	virtual BTNodeStatus Tick(T* context, float deltaTime) = 0;

	// Running中に親の判断で打ち切られた時の後始末(再生停止・経過時間のクリア等)。
	virtual void Reset() {}
};
