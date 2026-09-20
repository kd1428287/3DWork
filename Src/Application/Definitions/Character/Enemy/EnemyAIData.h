#pragma once
#include "../Common/CombatData.h"
#include "../Common/CharacterDefinitionCommon.h" // MotionClipData

// ============================================================
// 敵1体分の挙動パラメータをまとめたデータ。C++の型を分けるのではなく、
// このデータの中身を差し替えることで敵種(Brute/Boss/今後追加する敵)の
// 個体差を表現する(データ駆動)。EnemyAIController(唯一の実行クラス)は
// 全ての敵種でこのデータを変えるだけで共用する。
//
// 【この一般化について】
// 以前はBruteAIDataという名前でBrute専用のつもりで作ったが、実際には
// Brute固有の要素は何も無く、どんな敵にもそのまま使える内容だった。
// Boss等の継承ベース実装(BruteStatusController/BossStatusController)を
// 廃止しBTへ全面移行するにあたり、Brute専用という名前は実態と合わなく
// なったためEnemyAIDataへ改名した。
//
// 【movementAnimations/oneShotAnimationsについて】
// 以前は移動ループ("Idle"/"Walk"/"Run")や被弾リアクション・咆哮・死亡等の
// 単発演出のアニメーション名・尺が、EnemyActions.cpp/WarrockBehavior.cpp側に
// 直接ハードコードされていた(データ駆動という設計方針と矛盾していた)。
// 攻撃データ(EnemyAttackDefinition)と同じ扱いに揃えるため、この2つを
// EnemyAIData側のデータへ移す。実行層(EnemyActions/WarrockBehavior等)を
// 実際にこのデータを読む形へ配線する作業は、BT実行層の大規模リファクタリング
// で行う(現時点ではデータを持たせるところまで)。
// ============================================================

// 攻撃1種類分のデータ。ダメージ/体幹ダメージ、windup/active/recoveryの
// 3フェーズ構成、モーション制御、キャンセル設定等は汎用のAttackData
// (CombatData.h、Player側のAttackDataと共通)にまとめてあり、ここには
// BTの技選択(EnemyAIController/BTWeightedAttackAction<T>)にしか
// 使わない値だけを残す。
struct EnemyAttackDefinition
{
	std::string name = "Attack";

	AttackData attackData;

	// 有効間合い[minRange, maxRange]。範囲外なら選択候補にならない。
	float minRange = 0.0f;
	float maxRange = 2.0f;

	float weight = 1.0f;
};

// 常時ループする移動系アニメーションの名前。全敵種が必ず持つ役割
// なので固定フィールドとして持つ(oneShotAnimationsのような「敵種に
// よって有る/無い」という性質ではないため、名前引きのテーブルには
// しない)。Player側のMovementPhaseClips(Start/Loop/End)ほどの作り込みは
// 現状のBT実行層(ループ再生のみ)には不要なため、まずはLoop名だけを
// 持つ簡易版とする。Start/Endのブレンドが必要になった時点で
// Player側と同じ形へ拡張するかどうかを再検討すること。
struct EnemyMovementAnimationNames
{
	std::string idle = "Idle";
	std::string walk = "Walk";
	std::string run = "Run";
};

struct EnemyAIData
{
	// 巡回するウェイポイント(ワールド座標、この順番で巡回する)。
	// 空の場合、その場に立って待機し続けるだけの敵になる
	// (EnemyActionIdle::Tick()参照。ボスのように「持ち場を離れず
	// プレイヤーを待つ」敵はこれを空のままにする)。
	std::vector<Math::Vector3> patrolPoints;

	float patrolSpeed = 1.5f;
	float chaseSpeed = 3.0f;

	// 各ウェイポイントで足を止める時間(秒)。「待機」フェーズの長さ。
	float idleDuration = 1.5f;

	// この距離以内にターゲットが入ったら追跡を開始する。
	float detectionRange = 30.0f;

	// 追跡中、この距離より離れたら追跡を打ち切ってパトロールへ戻る。
	// detectionRangeより大きい値にしてヒステリシスを持たせることで、
	// 境界上でChase/Patrolを毎フレーム往復してしまう事故を防ぐ。
	float loseTargetRange = 9.0f;

	// EnemyActionMaintainDistance(EnemyActions.h参照)が使う、
	// ターゲットとの間合い。この距離以下まで近づいたら接近をやめて
	// その場に停止する(近すぎても後退はしない設計)。Chaseのように
	// 距離0まで詰めたくない敵(遠距離攻撃タイプ等)向けの汎用値。
	float maintainDistance = 3.0f;

	// 攻撃が1回終わってから、次の攻撃を選択できるようになるまでの
	// インターバル秒数(EnemyAIController::NotifyAttackCompleted()/
	// IsAttackOnCooldown()参照)。0ならインターバル無し(従来通り
	// Recovery終了直後から即座に次の攻撃を選べる)。
	// 「攻撃と攻撃の間」という敵の意思決定全体に関わる値であり、
	// 個々の技の性能ではないため、EnemyAttackDefinitionではなく
	// こちらに置く。インターバル中でも移動(追跡/巡回/待機)は
	// 制限されない(攻撃Sequenceの入り口だけで判定するため)。
	float attackIntervalDuration = 0.0f;

	// 攻撃パターン。近接プール(間合い内で重み付き抽選)。
	std::vector<EnemyAttackDefinition> attacks;

	// 間合い詰め専用プール。近接プールの範囲外(遠距離)にいる時だけ
	// 選択候補になる、別枠の技セット(BTWeightedAttackAction参照)。
	std::vector<EnemyAttackDefinition> gapCloserAttacks;

	// 巡回/追跡/待機で使うループアニメーションの名前。
	EnemyMovementAnimationNames movementAnimations;

	// 単発の割り込み演出(被弾リアクション/大スタン/パリィされた時の
	// リアクション/咆哮/死亡等)を論理名で引けるテーブル。
	// キー例: "HitReaction", "BigStagger", "Parried", "Roar", "Dying"
	//
	// 【mapにした理由】演出の種類は敵種ごとに増減する(Warrockには
	// Roarがあるが他の敵には無い、等)。EnemyAIDataに毎回専用フィールドを
	// 足す方式だと、このデータ構造自体が「新しい演出を増やすたびに
	// 触る」問題を抱えてしまうため、キーが存在しなければ「その敵種には
	// その演出が無い」として扱える名前引き方式にする。Behavior側は
	// find()/atで有無を見てからSequenceを組む想定。
	//
	// クリップの型はPlayer側のTurnAnimationSet等と共通のMotionClipData
	// (CharacterDefinitionCommon.h)を使う。以前はEnemy専用に
	// EnemyAnimationClip(animationName+durationのみ)を新設するつもり
	// だったが、Player側のMotionClipDataと概念が完全に重複していたため、
	// AttackDataをCombatData.hへ統合した時と同じ考え方でこちらへ寄せた
	// (useRootMotion/blendDurationも無償で使えるようになる)。
	std::unordered_map<std::string, MotionClipData> oneShotAnimations;

	// Dying演出(oneShotAnimations["Dying"].duration)の再生が終わってから、
	// 実際に消滅(RequestDespawn)するまでの余白秒数。唐突に消えないための
	// 「間」。旧WarrockBehavior::GetDespawnDelay()が独自定数として
	// 持っていた値をこちらへ移した。
	float postDeathLingerSeconds = 1.0f;
};
