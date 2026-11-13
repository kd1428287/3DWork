#pragma once

// ノードがTick()ごとに返す結果。親(Composite)がこれを見て子の実行順や打ち切りを決める。
enum class BTNodeStatus
{
	Success, // 完了して成功
	Failure, // 実行できない、または失敗
	Running, // 実行中(次フレームも継続)
};
