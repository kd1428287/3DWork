// PlayerCombatTypes.h
#pragma once

#include <string>
#include <cmath>

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

// --- 水平方向の角度計算(共通ヘルパー) ------------------------------
// forwardからinputDirへの、水平面(Y成分を無視)上での符号付き角度
// (-π〜+π、右回りを正)を返す。ClassifyEvadeDirection(4方向)と
// ClassifyMovementDirection8(8方向)の両方がこの角度をしきい値で
// 区切っているだけなので、座標系依存の右方向ベクトル計算をここへ
// 一本化しておく。
//
// 【要確認】right = (f.z, 0, -f.x) は、このプロジェクトの座標系
// (DirectX系、+Y上, +Z前方, +X右 の左手系を想定)でforwardを+90度(右へ)
// 回した向きになるはず、という前提で書いている。実際に鏡合わせになる場合は
// 符号を反転すること(TransformComponent::GetForward()の実装/RotationYの
// 回転方向と突き合わせて確認してください)。この前提が誤っていた場合、
// 修正箇所がここ1箇所で済むよう、Evade/Walk双方の分類関数はこの関数だけを
// 経由するようにしている。
inline float ComputeHorizontalAngleTo(const Math::Vector3& forward, const Math::Vector3& inputDir)
{
	Math::Vector3 f = forward;
	Math::Vector3 d = inputDir;
	f.y = 0.0f;
	d.y = 0.0f;
	if (f.LengthSquared() <= kDirectionEpsilon || d.LengthSquared() <= kDirectionEpsilon) {
		return 0.0f; // 前方/入力どちらかが実質ゼロベクトルならForward扱い(角度0)にする
	}
	f.Normalize();
	d.Normalize();

	const Math::Vector3 right(f.z, 0.0f, -f.x);
	return std::atan2(right.Dot(d), f.Dot(d));
}

// --- 回避方向の分類(4方向) -----------------------------------------
// 現在の前方(Enter()時点でFacingDirectionComponentの追従が止まった
// 直後の向き)に対して、入力方向(evadeDirection)がどちら寄りかを
// 4方向に分類したもの。前後左右で別アニメーションを出し分けるために使う。
//
// Evadeは今後もこの4方向のまま(前後左右)とする方針。斜め方向まで含めた
// 8方向のクリップを持つのはWalk側のみ(MovementDirection8参照)。
enum class EvadeDirection
{
	Forward = 0,
	Backward,
	Left,
	Right,
};

// forward: キャラクターの現在の前方(正規化済み想定、TransformComponent::GetForward())
// inputDir: 判定したい入力方向(PlayerInputComponent::PushCommand時点のスナップショット)。
//           ゼロベクトル(無入力でEvadeキーだけ押した場合)はForward扱いにする。
inline EvadeDirection ClassifyEvadeDirection(const Math::Vector3& forward, const Math::Vector3& inputDir)
{
	if (inputDir.LengthSquared() <= kDirectionEpsilon) {
		return EvadeDirection::Forward;
	}

	const float angle = ComputeHorizontalAngleTo(forward, inputDir);
	const float absAngle = std::abs(angle);

	// forwardとの前後判定。45度(≒0.7854rad)を閾値に前後/左右の4象限に分ける
	// (以前のcos45°によるdot比較と等価)。
	constexpr float kQuarter = static_cast<float>(M_PI) / 4.0f;   // 45度
	constexpr float kThreeQuarter = static_cast<float>(M_PI) * 3.0f / 4.0f; // 135度
	if (absAngle <= kQuarter)      return EvadeDirection::Forward;
	if (absAngle >= kThreeQuarter) return EvadeDirection::Backward;
	return (angle > 0.0f) ? EvadeDirection::Right : EvadeDirection::Left;
}

// --- 移動方向の分類(8方向、Walk専用) ---------------------------------
// EvadeDirectionとは別のenumとして独立させている。Evadeは今後も4方向の
// ままとする方針が確定しているため、同じ型を使い回すと「この値は
// 4方向のつもりか8方向のつもりか」が呼び出し側で曖昧になる。
enum class MovementDirection8
{
	Forward = 0,
	ForwardRight,
	Right,
	BackwardRight,
	Backward,
	BackwardLeft,
	Left,
	ForwardLeft,
};

// forward/inputDirの意味はClassifyEvadeDirectionと同じ。
// 45度ごとの8区画に丸める(境界は22.5度刻み)。
inline MovementDirection8 ClassifyMovementDirection8(const Math::Vector3& forward, const Math::Vector3& inputDir)
{
	if (inputDir.LengthSquared() <= kDirectionEpsilon) {
		return MovementDirection8::Forward;
	}

	const float angle = ComputeHorizontalAngleTo(forward, inputDir); // -π〜+π

	// 半セクター分(22.5度)オフセットしてから45度(=π/4)刻みで8分割する。
	// これにより、各セクターの境界がForward等の中心軸から±22.5度の
	// ところに来る(例: Forwardは[-22.5°, +22.5°)の範囲)。
	constexpr float kSectorSize = static_cast<float>(M_PI) / 4.0f;
	int sectorIndex = static_cast<int>(std::floor((angle + kSectorSize * 0.5f) / kSectorSize));
	sectorIndex = ((sectorIndex % 8) + 8) % 8; // 負値/8以上を0〜7へ正規化

	static constexpr MovementDirection8 kSectorTable[8] = {
		MovementDirection8::Forward,
		MovementDirection8::ForwardRight,
		MovementDirection8::Right,
		MovementDirection8::BackwardRight,
		MovementDirection8::Backward,
		MovementDirection8::BackwardLeft,
		MovementDirection8::Left,
		MovementDirection8::ForwardLeft,
	};
	return kSectorTable[sectorIndex];
}

// --- 技の1フェーズ(入り/中/終わり)ごとのアニメーション再生情報 -----------
// Attackの場合はWindup/Active/Recoveryの3フェーズにそのまま1:1で対応する
// (StateAttack::Enter()/Update()参照)。フェーズが切り替わるたびに、この
// 構造体の値をそのままPlayAnimation()へ渡す。
struct ActionPhaseData
{
	float duration = 0.2f;
	std::string animationName;
	bool useRootMotion = false;
	float blendDuration = 0.1f;
};

struct AttackMoveData
{
	ActionPhaseData windup;
	ActionPhaseData active;
	ActionPhaseData recovery;

	float stepDistance = 0.5f;
	float stepDuration = 0.1f;
	float engageDistance = 1.2f;

	// recovery.duration内で、ここから先は回避によるキャンセルを許可する
	// 開始タイミング(秒)。recovery.duration以上にすればキャンセル不可の技になる。
	float recoveryEvadeCancelStart = 0.15f;

	// recovery.duration内で、ここから先は次の攻撃(コンボ)によるキャンセルを
	// 許可する開始タイミング(秒)。recovery.duration以上にすればコンボ不可の技になる。
	float recoveryAttackCancelStart = 0.2f;

	Math::Vector3 stepDirection = Math::Vector3::Zero;
};

struct EvadeMoveData
{
	float activeDuration = 0.25f;
	float recoveryDuration = 0.15f;
	float justWindowStart = 0.05f;
	float justWindowEnd = 0.15f;
	float evadeDistance = 3.0f;
	Math::Vector3 evadeDirection = Math::Vector3::Zero;
	bool useRootMotion = false;

	// 前後左右で別クリップを再生するため、単一のanimationNameから分割。
	// 全方向とも同じクリップで良ければ4つとも同じ名前を入れればよい
	// (実アセットが揃うまでの暫定運用としてCreateDebugEvadeData()参照)。
	std::string animationNameForward = "GhostSamurai_APose_Slide_F_Inplace";
	std::string animationNameBackward = "GhostSamurai_APose_Slide_B_Inplace";
	std::string animationNameLeft = "GhostSamurai_APose_Slide_L_Inplace";
	std::string animationNameRight = "GhostSamurai_APose_Slide_R_Inplace";

	const std::string& GetAnimationName(EvadeDirection dir) const
	{
		switch (dir) {
		case EvadeDirection::Backward: return animationNameBackward;
		case EvadeDirection::Left:     return animationNameLeft;
		case EvadeDirection::Right:    return animationNameRight;
		case EvadeDirection::Forward:
		default:                       return animationNameForward;
		}
	}
};

// Walk(移動)アニメーションの8方向分岐用データ。以前は前後左右4方向
// (EvadeDirection)のみだったが、斜め移動時のクリップも持たせたいという
// 要望に伴い8方向(MovementDirection8)へ拡張した。
// Evade用ではなくWalk専用の型のため、EvadeMoveDataのような発動秒数や
// useRootMotion等は持たない(PlayerStatusController::walkAnimSet_参照)。
//
// 斜め方向のクリップが未用意の場合のフォールバックとして、既定値は
// 隣接する縦/横方向のクリップ名を流用している(実アセットが揃うまでの
// 暫定運用。CreateDebugEvadeData()と同じ考え方)。
struct DirectionalAnimationSet
{
	std::string forward = "GhostSamurai_APose_Strafe_Run_F_Loop_Inplace";
	std::string forwardRight = "GhostSamurai_APose_Strafe_Run_FR_Inplace";
	std::string right = "GhostSamurai_APose_Strafe_Run_R_Inplace";
	std::string backwardRight = "GhostSamurai_APose_Strafe_Run_BR_Inplace";
	std::string backward = "GhostSamurai_APose_Strafe_Run_B_Inplace";
	std::string backwardLeft = "GhostSamurai_APose_Strafe_Run_BL_Inplace";
	std::string left = "GhostSamurai_APose_Strafe_Run_L_Inplace";
	std::string forwardLeft = "GhostSamurai_APose_Strafe_Run_FL_Inplace";

	const std::string& GetAnimationName(MovementDirection8 dir) const
	{
		switch (dir) {
		case MovementDirection8::ForwardRight:  return forwardRight;
		case MovementDirection8::Right:         return right;
		case MovementDirection8::BackwardRight: return backwardRight;
		case MovementDirection8::Backward:      return backward;
		case MovementDirection8::BackwardLeft:  return backwardLeft;
		case MovementDirection8::Left:          return left;
		case MovementDirection8::ForwardLeft:   return forwardLeft;
		case MovementDirection8::Forward:
		default:                                return forward;
		}
	}
};

struct GuardMoveData
{
	// Guard開始からこの秒数以内に被弾すると、通常のブロックではなく
	// パリィ(弾き)として成立する。この秒数を過ぎたら以降は通常ブロック扱い。
	float justWindowDuration = 0.15f;

	std::string animationName = "GhostSamurai_APose2DefenseL_Inplace"; // 仮。単発再生+最終フレームでポーズ保持想定
};

// コンボ攻撃の最大段数。PlayerStatusController::comboAttacks_の要素数、
// および comboIndex_ の折り返し(% kMaxComboHits)に使う。
constexpr int kMaxComboHits = 5;