#pragma once

#include "EnemyAIData.h"
#include "../../Loaders/DefinitionJson.h" // Math::Vector3 / MotionClipData / AttackData のJSON変換
#include "Application/Definitions/Loaders/ComponentParamsOrderedJson.h"

// ============================================================
// EnemyAIData(Enemy1体分の挙動パラメータ)のJSON変換。
// EnemyAIパラメータとしてPrefab(JSON)から直接読み込むために使う。
// AttackData/MotionClipData/Math::Vector3はDefinitionJson.h側で
// 変換済みのため、ここではEnemyAIData固有の型だけを定義する。
// フィールド追加時はEnemyAIData.h側とこのファイル両方に足すこと。
//
// COMPONENT_PARAMS_DEFINE_TYPEの説明・理由はComponentParamsOrderedJson.hのコメントを参照。
// ============================================================

COMPONENT_PARAMS_DEFINE_TYPE(EnemyAttackDefinition,
	name, attackData, minRange, maxRange, weight)

COMPONENT_PARAMS_DEFINE_TYPE(EnemyMovementAnimationNames,
	idle, walk, run)

COMPONENT_PARAMS_DEFINE_TYPE(EnemyAIData,
	patrolPoints, patrolSpeed, chaseSpeed, idleDuration, detectionRange, loseTargetRange,
	maintainDistance, attackIntervalDuration, attacks, gapCloserAttacks,
	movementAnimations, oneShotAnimations, postDeathLingerSeconds)