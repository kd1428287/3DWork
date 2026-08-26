#pragma once
#include <array>
#include "PlayerCombatTypes.h"

// コンボ各段(kMaxComboHits分)のAttackMoveDataをまとめた型。
// PlayerStatusController::comboAttacks_と同じ型をここに集約しておくことで、
// 「コンボデータをどこかから読み込んで丸ごと差し替える」処理
// (デバッグ用直書き/将来のJSON読み込み)がこの型を単位にやり取りできる。
using ComboAttackTable = std::array<AttackMoveData, kMaxComboHits>;

// ============================================================
// デバッグ用: コンボ各段のAttackMoveDataをコード上に直書きして返す。
// JSON等の外部データ読み込みが未実装のための暫定処置
// (EnemyDefinitionDatabase::CreateDebugEnemyDatabase()と同じ考え方)。
// 実際の読み込み処理(JSON等)が決まったら、この関数を呼んでいる箇所
// (PlayerStatusController::Start()参照)をそちらに差し替えるだけで良い。
// ============================================================
ComboAttackTable CreateDebugComboAttackTable();

// ============================================================
// デバッグ用: Evade/Guardの基本データ(コンボのように段数は無く単一構成)を
// コード上に直書きして返す。中身は現状PlayerCombatTypes.h側の各構造体の
// デフォルト値と同じだが、「実際に使われる値はここ(CreateDebugXxx)が
// 出所である」という参照先を一本化しておくことで、将来JSON等から読み込む
// 関数に差し替える際に迷わないようにしている(PlayerCombatTypes.h側の
// デフォルト値は、万一読み込みに失敗した場合の安全側フォールバックとして
// 残す)。
// ============================================================
EvadeMoveData CreateDebugEvadeData();
GuardMoveData CreateDebugGuardData();