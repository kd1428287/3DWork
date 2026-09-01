#pragma once
#include "../EnemyAIData.h"

// ============================================================
// デバッグ用: コード上に直書きしたBruteAIDataを返す。
// JSON等の外部データ読み込みが決まったら、この関数の戻り値を
// そちらに差し替えるだけでよい(Player/EnemyのCreateDebugXxx()と同じ方針)。
// ============================================================
inline EnemyAIData CreateDebugBruteAIData()
{
	EnemyAIData data;

	data.patrolPoints = {
		Math::Vector3(3.0f, 0.0f, 0.0f),
		Math::Vector3(3.0f, 0.0f, 3.0f),
		Math::Vector3(0.0f, 0.0f, 3.0f),
		Math::Vector3(0.0f, 0.0f, 0.0f),
	};
	data.patrolSpeed = 1.5f;
	data.chaseSpeed = 3.0f;
	data.idleDuration = 1.5f;
	data.detectionRange = 6.0f;
	data.loseTargetRange = 9.0f;

	// 1: 近距離の速い一撃。
	EnemyAttackDefinition jab;
	jab.name = "Jab";
	jab.animationName = "BruteJab";
	jab.windupDuration = 0.2f;
	jab.activeDuration = 0.15f;
	jab.recoveryDuration = 0.3f;
	jab.maxRange = 1.5f;
	jab.weight = 2.0f; // 近距離では最も選ばれやすい
	data.attacks.push_back(jab);

	// 2: 中距離まで届く横薙ぎ。
	EnemyAttackDefinition sweep;
	sweep.name = "Sweep";
	sweep.animationName = "BruteSweep";
	sweep.windupDuration = 0.35f;
	sweep.activeDuration = 0.25f;
	sweep.recoveryDuration = 0.5f;
	sweep.maxRange = 2.5f;
	sweep.weight = 1.0f;
	data.attacks.push_back(sweep);

	// 3: 踏み込みを伴う長めのリーチの一撃。
	EnemyAttackDefinition lunge;
	lunge.name = "Lunge";
	lunge.animationName = "BruteLunge";
	lunge.windupDuration = 0.45f;
	lunge.activeDuration = 0.2f;
	lunge.recoveryDuration = 0.6f;
	lunge.maxRange = 3.5f;
	lunge.weight = 0.7f; // 隙が大きい分、選ばれる比率を下げている
	data.attacks.push_back(lunge);

	return data;
}
