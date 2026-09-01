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

	// 攻撃パターン。3種類程度を想定しているが、配列なのでいくつでも
	// 追加できる(拡張性の担保)。
	std::vector<EnemyAttackDefinition> attacks;
};

// ============================================================
// デバッグ用: コード上に直書きしたEnemyAIDataを返す。
// JSON等の外部データ読み込みが決まったら、この関数の戻り値を
// そちらに差し替えるだけでよい(Player/EnemyのCreateDebugXxx()と同じ方針)。
// ============================================================

// ============================================================
// デバッグ用: ボス想定のEnemyAIData。継承クラスを新設する代わりに、
// このデータの中身だけでBrute相当と別物の挙動を表現できることを示す例
// (patrolPointsを空にして「持ち場で待つ」ボスにしている)。
// ============================================================
inline EnemyAIData CreateDebugBossAIData()
{
	EnemyAIData data;

	// patrolPointsは意図的に空のまま。EnemyActionIdleが単独の待機AIとして
	// 機能し、プレイヤーが近づくまで持ち場から動かない。
	data.chaseSpeed = 4.0f;
	data.detectionRange = 10.0f;
	data.loseTargetRange = 14.0f;

	EnemyAttackDefinition slash;
	slash.name = "Slash";
	slash.animationName = "BossSlash";
	slash.windupDuration = 0.3f;
	slash.activeDuration = 0.25f;
	slash.recoveryDuration = 0.4f;
	slash.maxRange = 2.5f;
	slash.weight = 1.5f;
	data.attacks.push_back(slash);

	EnemyAttackDefinition slam;
	slam.name = "Slam";
	slam.animationName = "BossSlam";
	slam.windupDuration = 0.5f;
	slam.activeDuration = 0.3f;
	slam.recoveryDuration = 0.7f;
	slam.maxRange = 3.5f;
	slam.weight = 1.0f;
	data.attacks.push_back(slam);

	EnemyAttackDefinition chargeLunge;
	chargeLunge.name = "ChargeLunge";
	chargeLunge.animationName = "BossChargeLunge";
	chargeLunge.windupDuration = 0.6f;
	chargeLunge.activeDuration = 0.25f;
	chargeLunge.recoveryDuration = 0.9f;
	chargeLunge.maxRange = 5.0f;
	chargeLunge.weight = 0.6f; // 隙の大きい大技は選ばれる比率を下げる
	data.attacks.push_back(chargeLunge);

	return data;
}