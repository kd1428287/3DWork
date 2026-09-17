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
//
// 【oneShotAnimations/movementAnimationsについて】
// 以前はWarrockBehavior.cpp内の匿名namespace定数(kHitReactionDuration等)や
// アニメーション名のリテラル直書きとして持っていたが、攻撃データと
// 同じくデータ駆動に揃えるためこちらへ移した。実行層(WarrockBehavior.cpp)
// を実際にこのデータへ配線する作業は、BT実行層の大規模リファクタリングで
// 行う(現時点ではデータとして持たせるところまで。BTWeightedAttackAction
// がwindup/active/recoveryをまだ読んでいないのと同じ位置づけ)。
// ============================================================
inline EnemyAIData CreateDebugWarrockAIData()
{
	EnemyAIData data;

	// ボスは特定の間合いに立ち続け、プレイヤーが近づくまで待機する。
	// patrolPointsは意図的に空のまま(WarrockActionIdleがそもそも巡回を
	// 行わない実装のため、この値自体を参照しない。WarrockActions.h参照)。
	data.chaseSpeed = 2.0f;
	data.detectionRange = 10.0f;
	data.loseTargetRange = 14.0f;

	// 攻撃後、すぐ次の攻撃に移らないよう1秒のインターバルを設ける
	// (EnemyAIData::attackIntervalDuration参照)。値は暫定
	data.attackIntervalDuration = 0.5f;

	data.maintainDistance = 3.0f;

	// 現在実装済みの攻撃アニメーションはPunch/Kick/Swipning/JumpAttackの
	// 4種。Punchが最も軽く高頻度、JumpAttackが最も重く低頻度になるよう
	// windup/recoveryとweightを調整している。

	EnemyAttackDefinition punch;
	punch.name = "Punch";
	punch.attackData.phaseData.animationName = "Punch";
	punch.attackData.phaseData.windup.targetDuration = 0.5f;
	punch.attackData.phaseData.active.targetDuration = 0.4f;
	punch.attackData.phaseData.recovery.targetDuration = 0.7f;
	punch.maxRange = 3.0f;
	punch.weight = 2.0f;
	data.attacks.push_back(punch);

	EnemyAttackDefinition kick;
	kick.name = "Kick";
	kick.attackData.phaseData.animationName = "Kick";
	kick.attackData.phaseData.windup.targetDuration = 0.5f;
	kick.attackData.phaseData.active.targetDuration = 0.3f;
	kick.attackData.phaseData.recovery.targetDuration = 0.6f;
	kick.maxRange = 3.5f;
	kick.weight = 1.5f;
	data.attacks.push_back(kick);

	EnemyAttackDefinition swipe;
	swipe.name = "Swipe";
	swipe.attackData.phaseData.animationName = "Swipning";
	swipe.attackData.phaseData.windup.targetDuration = 0.7f;
	swipe.attackData.phaseData.active.targetDuration = 0.5f;
	swipe.attackData.phaseData.recovery.targetDuration = 0.9f;
	swipe.maxRange = 4.5f;
	swipe.weight = 1.0f;
	data.attacks.push_back(swipe);

	EnemyAttackDefinition jumpAttack;
	jumpAttack.name = "JumpAttack";
	jumpAttack.attackData.phaseData.animationName = "JumpAttack";
	jumpAttack.attackData.phaseData.windup.targetDuration = 0.9f;
	jumpAttack.attackData.phaseData.active.targetDuration = 0.4f;
	jumpAttack.attackData.phaseData.recovery.targetDuration = 1.8f;
	jumpAttack.maxRange = 5.0f;
	jumpAttack.weight = 0.6f; // 隙の大きい大技は選ばれる比率を下げる
	jumpAttack.attackData.moveData.useRootMotion = true;
	data.attacks.push_back(jumpAttack);

	// 移動アニメーション名。現状はデフォルトと同じ値だが、Warrock固有の
	// データであることを明示するためここで明示的に設定しておく。
	data.movementAnimations = { "Idle", "Walk", "Run" };

	// 単発の割り込み演出。旧WarrockBehavior.cpp内の匿名namespace定数
	// (kHitReactionDuration/kRoarDuration/kBigStaggerDuration等)は
	// ここへ移した。
	// BigStaggerは専用アニメーションが未実装のため、尺だけ変えて
	// HitReactionと同じ"SmallReaction"を暫定的に使い回す(旧コメントの
	// 経緯を引き継ぐ。専用アニメーションが用意でき次第差し替えること)。
	data.oneShotAnimations["HitReaction"] = { "SmallReaction", 0.4f };
	data.oneShotAnimations["BigStagger"] = { "SmallReaction", 3.0f };
	data.oneShotAnimations["Parried"] = { "SmallReaction", 0.4f };
	data.oneShotAnimations["Roar"] = { "Roaring", 1.5f };
	data.oneShotAnimations["Dying"] = { "Dying", 2.0f };

	// Dying演出(2.0秒)が終わってから、さらに1.0秒の余白を置いてから
	// 消滅させる(旧WarrockBehavior::GetDespawnDelay()の
	// kDyingDuration+kPostDeathLingerSecondsに相当)。
	data.postDeathLingerSeconds = 1.0f;

	return data;
}