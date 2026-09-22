#pragma once
#include "PlayerCombatTypes.h"

// ============================================================
// プレイヤーの「戦闘中の振る舞い」に関わるデータをまとめた単位。
//
// PlayerVisualDefinition(見た目)・PlayerCombatStatsDefinition(体幹/HP等の
// 数値)とは別に、攻撃(コンボ木)・回避・ガードの中身(秒数・アニメーション名・
// キャンセル受付タイミング等)をここに集約する。PlayerDefinitionが
// このデータをまとめて保持し、PlayerFactory経由でPlayerAttackSelector/
// PlayerStatusControllerへ配る。
//
// 【PlayerCombatDataTable.h/.cppとの関係】
// 旧PlayerCombatDataTable.h/.cppのCreateDebugEvadeData()/
// CreateDebugGuardData()が担っていた「コード上に直書きしたデバッグ用
// データ」の役目はこの構造体(のデフォルト値)へ統合する。
// CreateDebugComboAttackTable()は、コンボが固定長配列(ComboAttackTable=
// std::array<AttackData,1>)だった頃の名残でコメントアウトされたまま
// 残っており、コンボ木(PlayerAttackTable)への移行後は概念自体が
// attackTableに置き換わっている。
// この構造体をPlayerDefinitionLoaderで読み込む形に置き換えたら、
// PlayerCombatDataTable.h/.cppおよびPlayerStatusController::Awake()内の
// CreateDebugEvadeData()/CreateDebugGuardData()呼び出しは削除してよい
// (代わりにPlayerFactory経由で渡されたPlayerDefinition::combatBehaviorを
// 使う)。
// ============================================================
struct PlayerCombatBehaviorDefinition
{
	// コンボ木本体。PlayerAttackSelectorがそのまま保持する想定。
	// ロード直後に必ずPlayerAttackTable::Validate()を呼び、
	// id重複・comboLinksのリンク先不在・entryCommand重複が
	// 無いことを確認すること(不整合はテーブル定義側の誤り)。
	PlayerAttackTable attackTable;

	EvadeData evade;
	GuardData guard;
};

// ============================================================
// 移動(Walk/Run/その場ターン)のアニメーション定義一式。
//
// 【注意】現時点ではPlayerMovementAnimationComponent側の実装
// (アップロードされていないため未確認)がこれらの型(WalkAnimationSet/
// TurnAnimationSet/MovementPhaseClips、いずれもPlayerCombatTypes.h定義)を
// 実際にどう保持しているか(ハードコードか、既に何らかの形でデータ化
// 済みか)を確認したうえで配線すること。型自体はPlayerCombatTypes.hに
// 既に用意されているため、データの置き場所としてここへまとめておく。
// ============================================================
struct PlayerMovementAnimationDefinition
{
	WalkAnimationSet walk;
	TurnAnimationSet turn;

	// Run(常に入力方向へ正対して移動)のStart/Loop/End。
	// WalkAnimationSet::forwardと同じMovementPhaseClips構造を流用する。
	MovementPhaseClips run;

	std::string idle;
};

