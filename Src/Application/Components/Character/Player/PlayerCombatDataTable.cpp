#include "PlayerCombatDataTable.h"

ComboAttackTable CreateDebugComboAttackTable()
{
	ComboAttackTable table{};

	// 全て仮の数値。実際の手触りを見ながら調整する前提の初期値
	// (段が進むごとに、踏み込みが大きく・隙も大きくなる方向で仮に振っている)。
	//
	// アニメーションが入り/中/終わりの3クリップに分かれたことに伴い、
	// windup/active/recoveryそれぞれに個別のクリップ名・ブレンド時間・
	// ルートモーション有無を持たせている。クリップ名は仮の命名規則
	// ("AttackN_Windup"等)なので、実アセットの名前に差し替えること。

	// 1段目: 素早い差し込み
	table[0].windup = { 0.15f, "GhostSamurai_APose_Attack02_1_ALL_Inplace", false, 0.12f }; // Noneからの遷移なので通常よりやや長めのブレンド
	table[0].active = { 0.15f, "GhostSamurai_APose_Attack02_1_ALL_Inplacee", false, 0.08f };
	table[0].recovery = { 0.25f, "GhostSamurai_APose_Attack02_1_ALL_Inplace", false, 0.08f };
	table[0].stepDistance = 2.0f;
	table[0].stepDuration = 0.4f;
	table[0].engageDistance = 1.2f; // 差し込み技なので間合いはやや近め
	table[0].recoveryEvadeCancelStart = 0.10f;
	table[0].recoveryAttackCancelStart = 0.12f;

	// 2段目
	table[1].windup = { 0.18f, "GhostSamurai_APose_Attack02_1_ALL_Inplace", false, 0.08f }; // 前段からの継続。短めにして繋ぎの唐突さを緩和
	table[1].active = { 0.18f, "GhostSamurai_APose_Attack02_1_ALL_Inplace", false, 0.08f };
	table[1].recovery = { 0.30f, "GhostSamurai_APose_Attack02_1_ALL_Inplace", false, 0.08f };
	table[1].stepDistance = 1.6f;
	table[1].stepDuration = 0.4f;
	table[1].engageDistance = 1.2f;
	table[1].recoveryEvadeCancelStart = 0.15f;
	table[1].recoveryAttackCancelStart = 0.18f;

	// 3段目
	table[2].windup = { 0.20f, "GhostSamurai_APose_Attack02_1_ALL_Inplace", false, 0.08f };
	table[2].active = { 0.20f, "GhostSamurai_APose_Attack02_1_ALL_Inplace", false, 0.08f };
	table[2].recovery = { 0.32f, "GhostSamurai_APose_Attack02_1_ALL_Inplace", false, 0.08f };
	table[2].stepDistance = 1.6f;
	table[2].stepDuration = 0.4f;
	table[2].engageDistance = 1.3f;
	table[2].recoveryEvadeCancelStart = 0.16f;
	table[2].recoveryAttackCancelStart = 0.20f;

	// 4段目
	table[3].windup = { 0.22f, "GhostSamurai_APose_Attack02_1_ALL_Inplace", false, 0.08f };
	table[3].active = { 0.22f, "GhostSamurai_APose_Attack02_1_ALL_Inplace", false, 0.08f };
	table[3].recovery = { 0.35f, "GhostSamurai_APose_Attack02_1_ALL_Inplace", false, 0.08f };
	table[3].stepDistance = 1.7f;
	table[3].stepDuration = 0.4f;
	table[3].engageDistance = 1.3f;
	table[3].recoveryEvadeCancelStart = 0.18f;
	table[3].recoveryAttackCancelStart = 0.22f;

	// 5段目: フィニッシュ。recoveryAttackCancelStartをrecovery.durationと
	// 同じ値にすることで、コメント通り「recovery.duration以上ならコンボ
	// 不可の技になる」仕様を使い、5段目からは次のコンボへ継続できない
	// ようにしている(自然に手数が打ち切られ、Noneへ戻ってcomboIndex_が
	// 1段目にリセットされる)。ダメージ量等はAttackSourceComponent側の
	// 管轄のためここでは扱わない。
	//
	// フィニッシュ技はactiveフェーズ(実際に踏み込んで斬りつける瞬間)だけ
	// ルートモーション付きのクリップを使う想定【要確認: 実アセットが
	// どのフェーズにルートモーションを焼き込んでいるか要突き合わせ】。
	// activeがuseRootMotion=trueの間、stepDistance/stepDirection/
	// stepDuration/engageDistanceは無視される(StateAttack::Update参照)。
	table[4].windup = { 0.30f, "GhostSamurai_APose2DefenseL_Inplace", false, 0.1f };
	table[4].active = { 0.25f, "GhostSamurai_APose2DefenseL_Inplace", true, 0.1f };
	table[4].recovery = { 0.50f, "GhostSamurai_APose2DefenseL_Inplace", false, 0.1f };
	table[4].stepDistance = 1.4f;
	table[4].stepDuration = 0.6f;
	table[4].recoveryEvadeCancelStart = 0.30f;
	table[4].recoveryAttackCancelStart = 0.50f;

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

	data.animationNameForward	= "GhostSamurai_APose_Slide_F_Inplace";
	data.animationNameBackward	= "GhostSamurai_APose_Slide_B_Inplace";
	data.animationNameLeft		= "GhostSamurai_APose_Slide_L_Inplace";
	data.animationNameRight		= "GhostSamurai_APose_Slide_R_Inplace";

	return data;
}
GuardMoveData CreateDebugGuardData()
{
	GuardMoveData data{};
	data.justWindowDuration = 0.15f;
	data.animationName = "GhostSamurai_APose2DefenseL_Inplace";
	return data;
}