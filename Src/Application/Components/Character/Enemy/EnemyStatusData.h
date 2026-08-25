#pragma once

// ============================================================
// EnemyStatusController(および派生クラス)のチューニング値をまとめた
// データ構造。以前はEnemyStatusController内のコンストラクタ引数や
// static constexprとして直接コードに埋め込まれていたが、将来的に
// 敵の種類ごとの数値をJSON等の外部データから読み込めるようにするため、
// ここへ切り出している。
//
// 実際にJSONファイルから読み込む処理(LoadFromJson等)は、使用する
// JSONライブラリ/フォーマットがまだ分かっていないため未実装。
// 決まり次第、ここへstaticなロード関数を追加する想定
// (例: static EnemyStatusData LoadFromJson(const std::string& path);)。
// ============================================================
struct EnemyStatusData {
	// パトロールAI: 基準点からこの距離だけ離れたら折り返す
	float patrolDistance = 3.0f;

	// 仮の攻撃タイマー(実際の攻撃AIが実装されるまでの暫定値)。
	// 一定間隔(attackInterval)ごとに、attackActiveDurationの間だけ
	// 武器のHitBoxを有効化する。
	float attackInterval = 1.0f;
	float attackActiveDuration = 0.2f;

	// ノックバックの安全域クランプ(地面すり抜け対策)。
	// 詳細はEnemyStatusController::ClampKnockbackParams/
	// ClampKnockbackDirectionのコメント参照。
	float maxKnockbackSpeed = 15.0f;
	float minKnockbackDirectionY = -0.5f;
};
