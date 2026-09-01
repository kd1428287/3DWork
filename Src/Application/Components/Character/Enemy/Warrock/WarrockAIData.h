#pragma once
#include "../EnemyAIData.h"

// ============================================================
// デバッグ用: Warrock(ボス)想定のEnemyAIDataプリセット。
//
// 【EnemyAIData型を流用している理由】
// WarrockBehavior/WarrockActionsの判断層はBruteBehavior/EnemyActionsとは
// 完全に独立したWarrock専用の実装(WarrockBehavior.h冒頭コメント参照)。
// ここでEnemyAIData型を使っているのはロジックの共有が目的ではなく、
// EnemyDefinition::aiData(ファクトリー側が扱う共通のデータ構造)へ
// そのまま載せるための「データの器」を合わせているだけ。
//
// この関数は「EnemyAIDataという共通の器に、Warrock用の値を詰めて返す」
// だけの、CreateDebugBruteAIData()/CreateDebugBossAIData()
// (EnemyAIData.h参照)と同じ形のプリセット関数として実装する。
// ============================================================
inline EnemyAIData CreateDebugWarrockAIData()
{
	EnemyAIData data;

	// ボスは特定の間合いに立ち続け、プレイヤーが近づくまで待機する。
	// patrolPointsは意図的に空のまま(WarrockActionIdleがそもそも巡回を
	// 行わない実装のため、この値自体を参照しない。WarrockActions.h参照)。
	data.chaseSpeed = 4.0f;
	data.detectionRange = 10.0f;
	data.loseTargetRange = 14.0f;

	// 現在実装済みの攻撃アニメーションはPunch/Kick/Swipning/JumpAttackの
	// 4種。Punchが最も軽く高頻度、JumpAttackが最も重く低頻度になるよう
	// windup/recoveryとweightを調整している。

	EnemyAttackDefinition punch;
	punch.name = "Punch";
	punch.animationName = "Punch";
	punch.windupDuration = 0.25f;
	punch.activeDuration = 0.2f;
	punch.recoveryDuration = 0.35f;
	punch.maxRange = 2.0f;
	punch.weight = 2.0f;
	data.attacks.push_back(punch);

	EnemyAttackDefinition kick;
	kick.name = "Kick";
	kick.animationName = "Kick";
	kick.windupDuration = 0.3f;
	kick.activeDuration = 0.2f;
	kick.recoveryDuration = 0.4f;
	kick.maxRange = 2.5f;
	kick.weight = 1.5f;
	data.attacks.push_back(kick);

	EnemyAttackDefinition swipe;
	swipe.name = "Swipe";
	swipe.animationName = "Swipning";
	swipe.windupDuration = 0.5f;
	swipe.activeDuration = 0.3f;
	swipe.recoveryDuration = 0.7f;
	swipe.maxRange = 3.5f;
	swipe.weight = 1.0f;
	data.attacks.push_back(swipe);

	EnemyAttackDefinition jumpAttack;
	jumpAttack.name = "JumpAttack";
	jumpAttack.animationName = "JumpAttack";
	jumpAttack.windupDuration = 0.6f;
	jumpAttack.activeDuration = 0.25f;
	jumpAttack.recoveryDuration = 0.9f;
	jumpAttack.maxRange = 5.0f;
	jumpAttack.weight = 0.6f; // 隙の大きい大技は選ばれる比率を下げる
	jumpAttack.useRootMotion = true;
	data.attacks.push_back(jumpAttack);

	return data;
}
