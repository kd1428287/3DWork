// PlayerCombatTypes.h
#pragma once

#include <string>
#include <cmath>

// ============================================================
// PlayerStatusController / PlayerInputComponent 双方から参照される
// 型をまとめた共有ヘッダ。
// ============================================================

enum class MovementState
{
	Stand = 0,
	Walk,
	Run,
};

enum class CombatState
{
	None = 0,
	AttackWindup,
	AttackActive,
	AttackRecovery,
	Evade,
	EvadeRecovery,
	Guard,
	StaggerSmall,
	StaggerLarge,
};



enum class ActionCommand
{
	Attack,
	Evade,
};


// --- 前後左右4方向の分類 -----------------------------------------
// Evade(回避)専用。
enum class EvadeDirection
{
	Forward = 0,
	Backward,
	Left,
	Right,
};

inline EvadeDirection ClassifyEvadeDirection(const Math::Vector3& forward, const Math::Vector3& inputDir)
{
	if (inputDir.LengthSquared() <= kDirectionEpsilon) {
		return EvadeDirection::Forward;
	}

	const float angle = ComputeHorizontalAngleTo(forward, inputDir);
	const float absAngle = std::abs(angle);

	constexpr float kQuarter = static_cast<float>(M_PI) / 4.0f;
	constexpr float kThreeQuarter = static_cast<float>(M_PI) * 3.0f / 4.0f;
	if (absAngle <= kQuarter)      return EvadeDirection::Forward;
	if (absAngle >= kThreeQuarter) return EvadeDirection::Backward;
	return (angle > 0.0f) ? EvadeDirection::Right : EvadeDirection::Left;
}

// --- 前後左右8方向の分類 -----------------------------------------
// ロックオン中のWalk(歩行)専用。向き(facing)は固定されたまま、入力方向が
// その固定向きに対してどちらかを45度刻みの8方向で分類する。
enum class Direction8
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

inline Direction8 ClassifyDirection8(const Math::Vector3& forward, const Math::Vector3& inputDir)
{
	if (inputDir.LengthSquared() <= kDirectionEpsilon) {
		return Direction8::Forward;
	}

	const float angle = ComputeHorizontalAngleTo(forward, inputDir);
	constexpr float kSector = static_cast<float>(M_PI) / 4.0f;
	constexpr float kHalfSector = kSector / 2.0f;

	float normalized = angle;
	if (normalized < 0.0f) normalized += 2.0f * static_cast<float>(M_PI);
	const int sector = static_cast<int>((normalized + kHalfSector) / kSector) % 8;

	switch (sector) {
	case 0: return Direction8::Forward;
	case 1: return Direction8::ForwardRight;
	case 2: return Direction8::Right;
	case 3: return Direction8::BackwardRight;
	case 4: return Direction8::Backward;
	case 5: return Direction8::BackwardLeft;
	case 6: return Direction8::Left;
	case 7: return Direction8::ForwardLeft;
	default: return Direction8::Forward;
	}
}

// --- ターン(その場方向転換)の分類 -----------------------------------
// 非ロックオン中、Standから動き出す瞬間に「現在の向き→入力方向」への
// 回転が大きい場合、90度/180度の専用ターンアニメーションを挟む。
enum class TurnDirection
{
	None = 0,
	Left90,
	Right90,
	Left180,
	Right180,
};

inline TurnDirection ClassifyTurnDirection(const Math::Vector3& forward, const Math::Vector3& inputDir)
{
	if (inputDir.LengthSquared() <= kDirectionEpsilon) {
		return TurnDirection::None;
	}

	const float angle = ComputeHorizontalAngleTo(forward, inputDir);
	const float absAngle = std::abs(angle);

	constexpr float kQuarter = static_cast<float>(M_PI) / 4.0f;
	constexpr float kThreeQuarter = static_cast<float>(M_PI) * 3.0f / 4.0f;

	if (absAngle <= kQuarter) return TurnDirection::None;

	const bool isRight = (angle > 0.0f);
	if (absAngle >= kThreeQuarter) return isRight ? TurnDirection::Right180 : TurnDirection::Left180;
	return isRight ? TurnDirection::Right90 : TurnDirection::Left90;
}

// --- 技の1フェーズ(入り/中/終わり)ごとのアニメーション再生情報 -----------
struct ActionPhaseData
{
	float duration = 0.2f;
	std::string animationName;
	bool useRootMotion = false;
	float blendDuration = 0.1f;
};

struct AttackMoveData
{
	float windupDuration = 0.2f;
	float activeDuration = 0.25f;
	float recoveryDuration = 0.3f;
	float stepDistance = 0.5f;
	float stepDuration = 0.1f;
	float engageDistance = 1.2f;
	float recoveryEvadeCancelStart = 0.15f;
	float recoveryAttackCancelStart = 0.2f;
	Math::Vector3 stepDirection = Math::Vector3::Zero;
	float blendDuration = 0.1f;
	bool useRootMotion = false;
	std::string animationName = "Attack1";
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

// --- 移動(Walk/Run)の動き出し/継続/止まり際 3フェーズ分のアニメーション ---
struct MovementPhaseClips
{
	std::string startAnimationName;
	float startDuration = 0.15f;
	std::string loopAnimationName;
	std::string endAnimationName;
	float endDuration = 0.15f;
};

// ロックオン中のWalk(歩行)8方向分の、Loopのみのクリップ名。
// ロックオン中は向きを固定したまま8方向のいずれかを再生するだけで、
// Start/Endは持たない。
struct WalkLockedAnimationSet
{
	std::string forward;
	std::string forwardRight;
	std::string right;
	std::string backwardRight;
	std::string backward;
	std::string backwardLeft;
	std::string left;
	std::string forwardLeft;

	const std::string& Get(Direction8 dir) const
	{
		switch (dir) {
		case Direction8::ForwardRight:  return forwardRight;
		case Direction8::Right:         return right;
		case Direction8::BackwardRight: return backwardRight;
		case Direction8::Backward:      return backward;
		case Direction8::BackwardLeft:  return backwardLeft;
		case Direction8::Left:          return left;
		case Direction8::ForwardLeft:   return forwardLeft;
		case Direction8::Forward:
		default:                        return forward;
		}
	}
};

// Walk(歩行)のアニメーションデータ一式。
// forward: 非ロックオン中に常に使う、前進のStart/Loop/End
//          (非ロック時は入力方向へ向き直してから前進するため、
//           キャラクター視点では常に「前方」の1種類のみで済む)。
// locked:  ロックオン中、向きを固定したまま入力方向で切り替える8方向の
//          Loopのみのクリップ。
struct WalkAnimationSet
{
	MovementPhaseClips forward;
	WalkLockedAnimationSet locked;
};

// その場ターン(90度/180度、左右)のアニメーション。
struct TurnAnimationSet
{
	ActionPhaseData left90;
	ActionPhaseData right90;
	ActionPhaseData left180;
	ActionPhaseData right180;
};

struct GuardMoveData
{
	float justWindowDuration = 0.15f;
	std::string animationName = "GhostSamurai_APose2DefenseL_Inplace";
};

constexpr int kMaxComboHits = 6;