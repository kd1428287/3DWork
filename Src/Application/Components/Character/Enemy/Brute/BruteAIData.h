#pragma once
// ============================================================
// Brute1体分の挙動パラメータをまとめたデータ。C++の型を分けるのではなく、
// このデータの中身を差し替えることで個体差/バリエーションを表現する
// (データ駆動)。攻撃パターンは配列なので、種類を増やしたい場合は
// CreateDebugBruteAIData()等でattacksへ要素を追加するだけでよい。
// ============================================================

// 攻撃1種類分のデータ。拡張性を持たせるため、windup/active/recoveryの
// 3フェーズ構成にしている(Player/EnemyのAttackMoveDataと同じ考え方)。
struct BruteAttackDefinition
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
};

struct BruteAIData
{
	// 巡回するウェイポイント(ワールド座標、この順番で巡回する)。
	// 空の場合、Bruteはその場に立って待機し続けるだけになる
	// (BruteActionIdle::Tick()参照)。
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
	std::vector<BruteAttackDefinition> attacks;
};

// ============================================================
// デバッグ用: コード上に直書きしたBruteAIDataを返す。
// JSON等の外部データ読み込みが決まったら、この関数の戻り値を
// そちらに差し替えるだけでよい(Player/EnemyのCreateDebugXxx()と同じ方針)。
// ============================================================
inline BruteAIData CreateDebugBruteAIData()
{
	BruteAIData data;

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
	BruteAttackDefinition jab;
	jab.name = "Jab";
	jab.animationName = "BruteJab";
	jab.windupDuration = 0.2f;
	jab.activeDuration = 0.15f;
	jab.recoveryDuration = 0.3f;
	jab.maxRange = 1.5f;
	jab.weight = 2.0f; // 近距離では最も選ばれやすい
	data.attacks.push_back(jab);

	// 2: 中距離まで届く横薙ぎ。
	BruteAttackDefinition sweep;
	sweep.name = "Sweep";
	sweep.animationName = "BruteSweep";
	sweep.windupDuration = 0.35f;
	sweep.activeDuration = 0.25f;
	sweep.recoveryDuration = 0.5f;
	sweep.maxRange = 2.5f;
	sweep.weight = 1.0f;
	data.attacks.push_back(sweep);

	// 3: 踏み込みを伴う長めのリーチの一撃。
	BruteAttackDefinition lunge;
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
