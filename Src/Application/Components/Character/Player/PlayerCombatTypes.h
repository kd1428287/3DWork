// PlayerCombatTypes.h
#pragma once

#include <string>

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

// --- 回避方向の分類 -----------------------------------------------
// 現在の前方(Enter()時点でFacingDirectionComponentの追従が止まった
// 直後の向き)に対して、入力方向(evadeDirection)がどちら寄りかを
// 4方向に分類したもの。前後左右で別アニメーションを出し分けるために使う。
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
//
// 【要確認】right = (forward.z, 0, -forward.x) は、このプロジェクトの座標系
// (DirectX系、+Y上, +Z前方, +X右 の左手系を想定)でforwardを+90度(右へ)
// 回した向きになるはず、という前提で書いている。実際に鏡合わせになる場合は
// 符号を反転すること(TransformComponent::GetForward()の実装/RotationYの
// 回転方向と突き合わせて確認してください)。
inline EvadeDirection ClassifyEvadeDirection(const Math::Vector3& forward, const Math::Vector3& inputDir)
{
	if (inputDir.LengthSquared() <= kDirectionEpsilon) {
		return EvadeDirection::Forward;
	}

	// 水平面(Y)は無視して比較する。
	Math::Vector3 f = forward;
	Math::Vector3 d = inputDir;
	f.y = 0.0f;
	d.y = 0.0f;
	if (f.LengthSquared() <= kDirectionEpsilon || d.LengthSquared() <= kDirectionEpsilon) {
		return EvadeDirection::Forward;
	}
	f.Normalize();
	d.Normalize();

	// forwardとの前後判定。cos45°(≒0.7071)を閾値にして前後/左右の4象限に分ける。
	const float forwardDot = f.Dot(d);
	if (forwardDot >= 0.70710678f)  return EvadeDirection::Forward;
	if (forwardDot <= -0.70710678f) return EvadeDirection::Backward;

	const Math::Vector3 right(f.z, 0.0f, -f.x);
	return (right.Dot(d) >= 0.0f) ? EvadeDirection::Right : EvadeDirection::Left;
}

// --- 技ごとの時間的定義 --------------------------------------------
// 技データテーブルの設計(どこで保持し、どう選択するか)は別途詰める。
// 現状はPlayerStatusController内部でデフォルト値をそのまま使う暫定運用。

struct AttackMoveData
{
	float windupDuration = 0.2f;
	float activeDuration = 0.25f;
	float recoveryDuration = 0.3f;

	float stepDistance = 0.5f; //	攻撃入力時対象方向か入力方向に移動
	// Windupが終わった瞬間(AttackActiveへの切り替わり)に開始する
	// 踏み込み移動の所要時間(StateAttack::Update参照)。
	float stepDuration = 0.1f;

	// recoveryDuration内で、ここから先は回避によるキャンセルを許可する
	// 開始タイミング(秒)。recoveryDuration以上にすればキャンセル不可の技になる。
	float recoveryEvadeCancelStart = 0.15f;

	// recoveryDuration内で、ここから先は次の攻撃(コンボ)によるキャンセルを
	// 許可する開始タイミング(秒)。recoveryDuration以上にすればコンボ不可の技になる。
	float recoveryAttackCancelStart = 0.2f;

	Math::Vector3 stepDirection = Math::Vector3::Zero;

	// このAttackへ切り替わる際のクロスフェード時間(秒)。
	// ModelAnimatorComponent::SetBlendDuration()にそのまま渡す。
	// ModelAnimatorComponent側のデフォルト(0.15秒)は「今のポーズが
	// 安定している状態からの遷移」を想定した長さだが、コンボの2段目
	// 以降は「Recovery中の任意のタイミングで割り込まれる」ため、
	// 同じ長さでブレンドすると出発点が毎回不安定になり、繋がりが
	// 不自然に見えやすい。攻撃データ側でこの値を個別に持たせることで、
	// 段ごとに(あるいは今後の技ごとに)繋ぎの長さを調整できるようにする。
	float blendDuration = 0.1f;

	// trueの場合、この攻撃はTweenMoveComponentによる決め打ち移動
	// (stepDistance/stepDuration)ではなく、アニメーションクリップに
	// 焼き込まれたルートモーションで移動する(StateAttack::Update参照)。
	// この場合stepDistance/stepDirection/stepDurationは無視される。
	bool useRootMotion = false;

	// 再生するアニメーション名(仮)。コンボの段数ごとに異なる想定のため、
	// PlayerStatusController::Start()でcomboAttacks_の各要素へ
	// "Attack1"〜"Attack5"を仮で割り当てる。
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

	// 前後左右で別クリップを再生するため、単一のanimationNameから分割。
	// 全方向とも同じクリップで良ければ4つとも同じ名前を入れればよい
	// (実アセットが揃うまでの暫定運用としてCreateDebugEvadeData()参照)。
	std::string animationNameForward = "StandToRoll";
	std::string animationNameBackward = "StandToRoll";
	std::string animationNameLeft = "StandToRoll";
	std::string animationNameRight = "StandToRoll";

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

// 歩行(Walk)アニメーションの前後左右分岐用データ。EvadeMoveDataの
// animationNameForward等+GetAnimationName()と同じ考え方だが、Evadeのように
// 発動秒数やuseRootMotion等の付随データを持たないため、専用の軽量な型として
// 切り出す(PlayerStatusController::walkAnimSet_参照)。
struct DirectionalAnimationSet
{
	std::string forward = "ForwardWalk";
	std::string backward = "BackWalk";
	std::string left = "LeftWalk";
	std::string right = "RightWalk";

	const std::string& GetAnimationName(EvadeDirection dir) const
	{
		switch (dir) {
		case EvadeDirection::Backward: return backward;
		case EvadeDirection::Left:     return left;
		case EvadeDirection::Right:    return right;
		case EvadeDirection::Forward:
		default:                       return forward;
		}
	}
};

struct GuardMoveData
{
	// Guard開始からこの秒数以内に被弾すると、通常のブロックではなく
	// パリィ(弾き)として成立する。この秒数を過ぎたら以降は通常ブロック扱い。
	float justWindowDuration = 0.15f;

	std::string animationName = "Guard"; // 仮。単発再生+最終フレームでポーズ保持想定
};

// コンボ攻撃の最大段数。PlayerStatusController::comboAttacks_の要素数、
// および comboIndex_ の折り返し(% kMaxComboHits)に使う。
constexpr int kMaxComboHits = 5;