#pragma once
#include <string>
#include <vector>

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
// ============================================================

// 攻撃1種類分のデータ。拡張性を持たせるため、windup/active/recoveryの
// 3フェーズ構成にしている(Player/EnemyのAttackMoveDataと同じ考え方)。
struct EnemyAttackDefinition
{
	std::string name = "Attack";
	std::string animationName = "Attack1";

	float windupDuration = 0.3f;
	float activeDuration = 0.2f;
	float recoveryDuration = 0.4f;

	// ターゲットとの距離がこの値以下の場合にだけ選択候補になる。
	// 技ごとに間合いを分けることで「近距離用の速い技」「中距離まで
	// 届くリーチのある技」等のバリエーションを表現できる。
	float maxRange = 2.0f;

	// 候補が複数ある場合の重み付き抽選に使う(値が大きいほど選ばれやすい)。
	float weight = 1.0f;

	// この技をアニメーションクリップのルートモーションで動かすか
	// (PlayerのAttackMoveData::useRootMotionと同じ考え方)。trueの場合、
	// BTWeightedAttackAction<T>がEnemyAIController::PlayAnimation()へ
	// この値を渡し、ModelAnimatorComponent側のルートモーション抽出が
	// 有効化される(EnemyAIController::ApplyRootMotion()参照)。
	// JumpAttackのような踏み込みを伴う技での利用を想定しているが、
	// 実アセットにルートモーションが焼き込まれていない技はfalseのまま
	// でよい。
	bool useRootMotion = false;
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
	float detectionRange = 6.0f;

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

	// 攻撃パターン。3種類程度を想定しているが、配列なのでいくつでも
	// 追加できる(拡張性の担保)。
	std::vector<EnemyAttackDefinition> attacks;
};