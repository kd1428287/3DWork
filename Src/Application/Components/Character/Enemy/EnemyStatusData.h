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

	// 攻撃1回分の時間パラメータ。以前はattackIntervalによる「一定間隔で
	// 自動発生」方式だったが、BT(EnemyBTController)が開始タイミングを
	// 判断するようになったため撤去し、代わりに「BTが開始を選んだ時に
	// 実行する単発攻撃」のWindup/Active/Recoveryとして再構成した
	// (PlayerのAttackMoveDataと同じ3フェーズ構成。コンボは持たない)。
	// attackActiveDuration(HitBoxが有効な時間)は以前と同じ意味のまま
	// 流用している。
	float attackWindupDuration = 0.4f;
	float attackActiveDuration = 0.2f;
	float attackRecoveryDuration = 0.5f;

	// BT側(EnemyBTController::IsPlayerInAttackRange())が、この間合い
	// 以内にいる時だけ攻撃を試みる。
	float attackRange = 2.0f;

	// ノックバックの安全域クランプ(地面すり抜け対策)。
	// 詳細はEnemyStatusController::ClampKnockbackParams/
	// ClampKnockbackDirectionのコメント参照。
	float maxKnockbackSpeed = 15.0f;
	float minKnockbackDirectionY = -0.5f;
};
