#pragma once

// ============================================================
// EnemyBlackboardKeys
//
// EnemyAIController::GetBlackboard()に書き込まれる、Enemyの動的な
// 実行時状態のキー名一覧。EnemyAIData.h(敵の静的パラメータのスキーマ)
// と対になる、敵の動的状態のスキーマ。
//
// 【ここに定数として集約している理由】
// キー名を文字列リテラルとして各所に直書きすると、書き込み側
// (EnemyAIController::Update()等)と読み出し側(BTのCondition/Action、
// 将来のノードグラフエディタのキー選択UI)でタイポにより食い違う
// 事故が起きうる。定数化することでコンパイル時に検出できるようにし、
// 同時にこのファイル自体が「このBlackboardには何が乗っているか」の
// 一覧表としても機能する(ノードグラフエディタのキー選択ドロップダウンは、
// 将来的にこの一覧を列挙する形で実装する想定)。
//
// 【値の型について】
// 各キーの型はBlackboard::Value(bool/float/std::string)のいずれかに
// 対応する。ここではコメントで明記するに留め、型自体の強制は
// 呼び出し側の書き方(SetBool/SetFloat/SetString)に委ねる。
// ============================================================
namespace EnemyBlackboardKeys
{
	// bool。索敵のヒステリシス込みで「今ターゲットを捕捉しているか」。
	// EnemyAIController::UpdateTargetAcquisition()が毎フレーム書き込む。
	constexpr const char* HasTarget = "HasTarget";

	// float。ターゲットまでの距離。HasTarget==falseの間は意味を持たない
	// 値(未捕捉時はFLT_MAX相当)になりうるため、呼び出し側は必ず
	// HasTargetを併せて確認すること。
	// EnemyAIController::UpdateTargetAcquisition()が毎フレーム書き込む。
	constexpr const char* DistanceToTarget = "DistanceToTarget";

	// bool。攻撃のインターバル中(EnemyAIData::attackIntervalDuration)で、
	// 新たな攻撃を選択できない状態かどうか。EnemyAIController::Update()が
	// 毎フレーム書き込む。
	constexpr const char* IsAttackOnCooldown = "IsAttackOnCooldown";

	// bool。現在のターゲットが、いずれかの攻撃(EnemyAIData::attacks)の
	// maxRange以内にいるか。EnemyAIController::Update()が
	// (EnemyAIController::IsTargetInAttackRange()の計算結果を)毎フレーム
	// 書き込む。
	constexpr const char* IsTargetInAttackRange = "IsTargetInAttackRange";

	// bool。ターゲットがgapCloserAttacksのいずれかの間合い内にいるか。
	constexpr const char* IsTargetInGapCloserRange = "IsTargetInGapCloserRange";

	// bool。各IEnemyBehavior実装が「今何らかの割り込み演出(被弾
	// リアクション/大スタン/パリィ/咆哮等)を要求中か」を書き込むための
	// 汎用キー。具体的にどの演出かという詳細(WarrockBehavior::pending_
	// 相当)まではBlackboardへは公開せず、Behavior側の内部実装に留める
	// (BTPendingReactionAction<T>はこの詳細をBehaviorへ問い合わせる
	// 専用のラムダ経由で読むため、Blackboard越しにする必要が無い)。
	constexpr const char* HasPendingReaction = "HasPendingReaction";
}
