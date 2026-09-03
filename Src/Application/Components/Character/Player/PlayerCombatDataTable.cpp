#include "PlayerCombatDataTable.h"

ComboAttackTable CreateDebugComboAttackTable()
{
	ComboAttackTable table{};

	// 全て仮の数値。実際の手触りを見ながら調整する前提の初期値
	// (段が進むごとに、踏み込みが大きく・隙も大きくなる方向で仮に振っている)。

	// 1段目: 素早い差し込み
	table[0].windupDuration = 0.15f;
	table[0].activeDuration = 0.15f;
	table[0].recoveryDuration = 0.25f;
	table[0].stepDistance = 2.0f;
	table[0].stepDuration = 0.4f;
	table[0].engageDistance = 1.2f; // 差し込み技なので間合いはやや近め
	table[0].recoveryEvadeCancelStart = 0.10f;
	table[0].recoveryAttackCancelStart = 0.12f;
	table[0].blendDuration = 0.12f; // Noneからの遷移なので通常よりやや長め
	table[0].animationName = "Attack1";

	// 2段目
	table[1].windupDuration = 0.18f;
	table[1].activeDuration = 0.18f;
	table[1].recoveryDuration = 0.30f;
	table[1].stepDistance = 1.6f;
	table[1].stepDuration = 0.4f;
	table[1].engageDistance = 1.2f;
	table[1].recoveryEvadeCancelStart = 0.15f;
	table[1].recoveryAttackCancelStart = 0.18f;
	table[1].blendDuration = 0.08f; // 前段からの継続。短めにして繋ぎの唐突さを緩和
	table[1].animationName = "Attack2";

	// 3段目
	table[2].windupDuration = 0.20f;
	table[2].activeDuration = 0.20f;
	table[2].recoveryDuration = 0.32f;
	table[2].stepDistance = 1.6f;
	table[2].stepDuration = 0.4f;
	table[2].engageDistance = 1.3f;
	table[2].recoveryEvadeCancelStart = 0.16f;
	table[2].recoveryAttackCancelStart = 0.20f;
	table[2].blendDuration = 0.08f;
	table[2].animationName = "Attack3";

	// 4段目
	table[3].windupDuration = 0.22f;
	table[3].activeDuration = 0.22f;
	table[3].recoveryDuration = 0.35f;
	table[3].stepDistance = 1.7f;
	table[3].stepDuration = 0.4f;
	table[3].engageDistance = 1.3f;
	table[3].recoveryEvadeCancelStart = 0.18f;
	table[3].recoveryAttackCancelStart = 0.22f;
	table[3].blendDuration = 0.08f;
	table[3].animationName = "Attack4";

	// 5段目: フィニッシュ。recoveryAttackCancelStartをrecoveryDurationと
	// 同じ値にすることで、コメント通り「recoveryDuration以上ならコンボ
	// 不可の技になる」仕様を使い、5段目からは次のコンボへ継続できない
	// ようにしている(自然に手数が打ち切られ、Noneへ戻ってcomboIndex_が
	// 1段目にリセットされる)。ダメージ量等はAttackSourceComponent側の
	// 管轄のためここでは扱わない。
	table[4].windupDuration = 0.30f;
	table[4].activeDuration = 0.25f;
	table[4].recoveryDuration = 0.50f;
	table[4].stepDistance = 1.4f;
	table[4].stepDuration = 0.6f;
	table[4].recoveryEvadeCancelStart = 0.30f;
	table[4].recoveryAttackCancelStart = 0.50f;
	table[4].blendDuration = 0.1f; // フィニッシュ技。次はNoneへ戻るだけなので通常寄りの値
	// フィニッシュ技はルートモーション付きのクリップを使う想定。
	// stepDistance/stepDirection/stepDuration/engageDistanceはuseRootMotion=true
	// の間無視される(StateAttack::Update参照)。
	table[4].useRootMotion = true;
	table[4].animationName = "Attack5";

	return table;
}

EvadeMoveData CreateDebugEvadeData()
{
	EvadeMoveData data{};
	data.activeDuration = 0.25f;
	data.recoveryDuration = 0.15f;
	data.justWindowStart = 0.05f;
	data.justWindowEnd = 0.15f;
	data.evadeDistance = 3.0f;
	data.useRootMotion = true;

	data.animationNameForward = "ForwardStep";
	data.animationNameBackward = "BackStep";
	data.animationNameLeft = "LeftStep";
	data.animationNameRight = "RightStep";

	return data;
}
GuardMoveData CreateDebugGuardData()
{
	GuardMoveData data{};
	data.justWindowDuration = 0.15f;
	data.animationName = "Guard";
	return data;
}