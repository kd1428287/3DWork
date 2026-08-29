#pragma once
#include "BTNodeStatus.h"

// ============================================================
// ビヘイビアツリーのノード基底インターフェース。
// PlayerState.hのIState<PlayerStatusController>と同じ考え方で、
// Tを「このツリーがぶら下がるController型」として渡すことで、
// ノード側からController(EnemyBTController等)の公開APIを直接呼び出せる
// (Stateが具体的なコンポーネントを直接知らずController経由で操作するのと
//  同じ責務分担)。
// ============================================================
template <typename T>
class IBTNode
{
public:
	virtual ~IBTNode() = default;

	// 毎フレーム評価する。deltaTimeは経過時間(秒)。
	// Runningを返した場合、次フレームも同じノードから再評価される想定
	// (Composite側は「前回Runningだったノードから再開する」実装にする。
	//  BTComposite.h参照)。
	virtual BTNodeStatus Tick(T* context, float deltaTime) = 0;

	// 実行中に他の枝へ切り替わる等で中断された場合の後始末
	// (再生中のアニメーションの停止、内部の経過時間のクリア等)。
	// Running状態のノードが、親の判断で打ち切られた際にだけ呼ばれる想定。
	virtual void Reset() {}
};
