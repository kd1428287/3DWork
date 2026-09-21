#pragma once

#include "EnemyAIData.h"
#include "../../Loaders/DefinitionJson.h" // Math::Vector3 / MotionClipData / AttackData のJSON変換

// ============================================================
// EnemyAIData(Enemy1体分の挙動パラメータ)のJSON変換。
// EnemyAIパラメータとしてPrefab(JSON)から直接読み込むために使う。
// AttackData/MotionClipData/Math::Vector3はDefinitionJson.h側で
// 変換済みのため、ここではEnemyAIData固有の型だけを定義する。
// フィールド追加時はEnemyAIData.h側とこのファイル両方に足すこと。
// ============================================================

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EnemyAttackDefinition,
	name, attackData, minRange, maxRange, weight)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EnemyMovementAnimationNames,
	idle, walk, run)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EnemyAIData,
	patrolPoints, patrolSpeed, chaseSpeed, idleDuration, detectionRange, loseTargetRange,
	maintainDistance, attackIntervalDuration, attacks, gapCloserAttacks,
	movementAnimations, oneShotAnimations, postDeathLingerSeconds)
