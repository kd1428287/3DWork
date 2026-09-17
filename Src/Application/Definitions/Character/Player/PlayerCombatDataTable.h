#pragma once
#include "PlayerCombatBehaviorDefinition.h"

// ============================================================
// デバッグ用: PlayerCombatBehaviorDefinition(コンボ木/回避/ガード)・
// PlayerMovementAnimationDefinition(歩行/走行/ターンのアニメーション)を
// コード上に直書きして返す。JSON等の外部データ読み込み(PlayerDefinitionLoader)
// が未対応の間の暫定データ源
// (EnemyDefinitionDatabase::CreateDebugEnemyDatabase()と同じ考え方)。
//
// 実際の読み込み処理(JSON等)が決まったら、この関数を呼んでいる箇所を
// そちらに差し替えるだけで良い(PlayerDefinitionLoader::LoadFromFile内で、
// ファイル側にcombatBehavior/movementAnimationsのセクションが無い場合の
// フォールバックとして呼ぶ形が想定として自然)。
//
// 【旧内容との対応】
//   旧CreateDebugEvadeData()/CreateDebugGuardData()
//     → CreateDebugPlayerCombatBehavior()内のevade/guard
//   旧PlayerAttackSelector.cpp内のMakeComboHit()/BuildDebugAttackTable()
//     → CreateDebugPlayerCombatBehavior()内のattackTable
//   旧PlayerMovementAnimationComponent.h内のwalkAnimSet_/runAnimSet_/
//   turnAnimSet_のメンバ初期化子
//     → CreateDebugPlayerMovementAnimations()
//   旧ComboAttackTable(固定長配列)/CreateDebugComboAttackTable()は
//     コンボ木(PlayerAttackTable)への移行に伴い既に役目を終えており、
//     このファイルを書き換えた際に削除した。
// ============================================================
PlayerCombatBehaviorDefinition CreateDebugPlayerCombatBehavior();
PlayerMovementAnimationDefinition CreateDebugPlayerMovementAnimations();
