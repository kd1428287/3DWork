#pragma once

// ============================================================
// PlayerStatusController / PlayerInputComponent 双方から参照される
// 型をまとめた共有ヘッダ。
//
// 「StatusController が InputComponent を直接参照する」
// 「InputComponent は MovementState/ActionCommand を扱う」という
// 依存関係を両方満たそうとすると、互いのクラス定義を直接includeし合う
// 循環includeになってしまう。そのため、enum/データ構造だけを
// ここに独立させて、両者から一方的にincludeする形にしている。
// ============================================================

// --- 移動軸 -----------------------------------------------------
// CombatStateがNone以外の間(攻撃/回避/ガード/パリィ/怯み中)は
// 参照されない想定。その間の位置移動は各モーション側が直接扱う。
enum class MovementState
{
	Stand = 0,
	Walk,
	Run,
};

// --- 戦闘軸 -----------------------------------------------------
// 「予備動作→判定中→硬直→(None or 次の行動へキャンセル)」という
// 攻撃/回避のサイクルと、押しっぱなしで継続するGuard、
// 割り込みで遷移してくるStaggerを持つ。
//
// パリィ(弾き)は独立した状態を持たない。Guard開始直後の数フレームに
// 判定が乗っている「時間窓」に過ぎないため、Guard状態のelapsed_と
// GuardMoveData::justWindowDurationの比較だけで表現する
// (StateGuard::GetGuardPhase()参照)。
enum class CombatState
{
	None = 0,
	AttackWindup,
	AttackActive,
	AttackRecovery,
	Evade,
	EvadeRecovery,
	Guard,  // 押しっぱなしの継続状態。離すまで居座る。開始直後は弾き判定を伴う
	StaggerSmall,
	StaggerLarge,
};

// PlayerInputComponentの先行入力バッファに積む、単発(タップ)入力。
// Guardは押しっぱなしの継続状態なのでここには含めない
// (IsGuardHeld()で都度参照する)。パリィもGuard開始に内包されるため
// 独立したコマンドを持たない。
// ゼロベクトル判定に使う共通の閾値。入力方向の正規化可否や
// 「実質無入力かどうか」の判定など、複数箇所で同じ基準を使いたいためここに置く。
constexpr float kDirectionEpsilon = 0.0001f;

enum class ActionCommand
{
	Attack,
	Evade,
};

// --- 技ごとの時間的定義 --------------------------------------------
// 技データテーブルの設計(どこで保持し、どう選択するか)は別途詰める。
// 現状はPlayerStatusController内部でデフォルト値をそのまま使う暫定運用。

struct AttackMoveData
{
	float windupDuration = 0.2f;
	float activeDuration = 0.15f;
	float recoveryDuration = 0.3f;

	float stepDistance = 0.5f; //	攻撃入力時対象方向か入力方向に移動
	float stepDuration = 0.1f; //	踏み込み速度

	// recoveryDuration内で、ここから先は回避によるキャンセルを許可する
	// 開始タイミング(秒)。recoveryDuration以上にすればキャンセル不可の技になる。
	float recoveryEvadeCancelStart = 0.15f;

	// recoveryDuration内で、ここから先は次の攻撃(コンボ)によるキャンセルを
	// 許可する開始タイミング(秒)。recoveryDuration以上にすればコンボ不可の技になる。
	float recoveryAttackCancelStart = 0.2f;

	Math::Vector3 stepDirection = Math::Vector3::Zero;
};

struct EvadeMoveData
{
	float activeDuration = 0.25f;    // 無敵が乗っている実移動フェーズ
	float recoveryDuration = 0.15f;  // 後隙(無敵切れ)

	// activeDuration内で、ここから先が「ジャスト回避」の成立窓。
	float justWindowStart = 0.05f;
	float justWindowEnd = 0.15f;

	// 回避で移動する距離。TweenMoveComponentでの決め打ち軌道に使う。
	float evadeDistance = 3.0f;

	// 回避方向。入力バッファに積まれた瞬間の移動入力を正規化して
	// スナップショットしたもの(PlayerInputComponent::PushCommand参照)。
	// 無入力(ゼロベクトル)の場合は、使う側(PlayerStatusController::RequestStepMove)で
	// モデルの向いている方向にフォールバックする。
	Math::Vector3 evadeDirection = Math::Vector3::Zero;
};

struct GuardMoveData
{
	// Guard開始からこの秒数以内に被弾すると、通常のブロックではなく
	// パリィ(弾き)として成立する。この秒数を過ぎたら以降は通常ブロック扱い。
	float justWindowDuration = 0.15f;
};
