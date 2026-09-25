#pragma once

#include "../Common/CombatData.h"
#include "../Common/CharacterDefinitionCommon.h"
#include "Application/Components/GamePlay/Character/Common/CharacterInputBufferComponent.h"

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
	Charge,
};



// Playerが「先行入力として覚えておきたいコマンド」の語彙。
// CharacterInputBufferComponent<TCommand>(../Common/
// CharacterInputBufferComponent.h)はコマンドの語彙に依存しない汎用の
// 入れ物であり、この列挙型自体はPlayer固有の値としてここに置く
// (将来Enemy AIが同じ仕組みを使う場合は、EnemyCommandのような
//  別の語彙をEnemy側のヘッダに定義し、
//  CharacterInputBufferComponent<EnemyCommand>を使えばよい。
//  Playerの語彙をEnemy側へ流用する必要は無い)。
enum class ActionCommand
{
	Attack,
	Evade,
	Guard,
	ChargeAttack, // Attack+Guard同時押しで溜め開始→離して発射する技のエントリ用
};

// Player用の入力バッファコンポーネントの具体化。
using PlayerActionBufferComponent = CharacterInputBufferComponent<ActionCommand>;


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

// このコンボ内での「次の技」候補1件分。
struct ComboLink
{
	// この入力コマンドが来たときにこのリンクを辿る候補になる。
	ActionCommand requiredCommand = ActionCommand::Attack;

	// 同じrequiredCommandを持つLinkが複数ある場合、値が小さい方から
	// 順に(コントローラー側の追加条件と合わせて)チェックし、最初に
	// 成立したものを採用する。分岐が無いなら既定の0のままでよい。
	int priority = 0;

	// 繋がる先の攻撃id(PlayerAttackDefinition::id)。存在しないidを
	// 指定した場合はPlayerAttackTable::Validate()で検出する。
	std::string nextAttackId;
};

// Player用の1技分のデータ。EnemyAttackDefinition(EnemyAIData.h)と
// 対になる構成で、汎用のAttackDataをメンバに持ち、Player固有の
// コンボ接続情報を添える。
struct PlayerAttackDefinition
{
	std::string id; // PlayerAttackTable内でユニークな識別子

	AttackData attack; // 汎用の攻撃データ(CombatData.h、Enemyと共通)

	// Recovery中(attack.cancelData.comboWindowAfterRecovery込み)に
	// 入力があった場合、ここに列挙されたLinkのうち条件に合うものへ
	// 遷移する。空なら、ここでコンボが終了する
	// (単発技・フィニッシュ技はこれで表現する)。
	std::vector<ComboLink> comboLinks;

	// Idle(非コンボ中)から、この攻撃へ直接入るための入力コマンド。
	// 未設定(std::nullopt)なら、他の攻撃のcomboLinks経由でしか
	// 辿り着けない技として扱う(=分岐でしか出さない派生フィニッシュ用)。
	std::optional<ActionCommand> entryCommand = ActionCommand::Attack;
};

// Playerが持つ全攻撃データの一覧。PlayerStatusControllerはこれを
// 1つ保持し、現在の攻撃id(comboIndex_の代わり)を文字列で追跡する形に
// なる。
struct PlayerAttackTable
{
	std::vector<PlayerAttackDefinition> attacks;

	const PlayerAttackDefinition* Find(const std::string& id) const
	{
		auto it = std::find_if(attacks.begin(), attacks.end(),
			[&](const PlayerAttackDefinition& a) { return a.id == id; });
		return it != attacks.end() ? &*it : nullptr;
	}

	// id重複・comboLinksのリンク先不在・entryCommandの重複が無いかを
	// チェックする(デバッグビルドでのassert等、テーブル構築直後に
	// 呼ぶ想定)。falseの場合、テーブルの定義側に不整合がある。
	bool Validate() const
	{
		for (size_t i = 0; i < attacks.size(); ++i) {
			for (size_t j = i + 1; j < attacks.size(); ++j) {
				if (attacks[i].id == attacks[j].id) return false;
			}
			for (const ComboLink& link : attacks[i].comboLinks) {
				if (Find(link.nextAttackId) == nullptr) return false;
			}
		}
		return true;
	}
};

// --- 回避1回分のデータ全体 -----------------------------------------
// (旧名EvadeMoveData。中身はevadeDistance/evadeDirection以外にも
// 秒数・ジャスト回避判定窓・方向別アニメーション名まで含んでおり、
// 「Move(移動)」だけを表す名前ではなくなっていたため、AttackData/
// GuardDataと同じ「〜Data」の命名に揃えた。)
struct EvadeData
{
	float activeDuration = 0.25f;
	float recoveryDuration = 0.15f;
	float justWindowStart = 0.05f;
	float justWindowEnd = 0.15f;
	float evadeDistance = 3.0f;
	Math::Vector3 evadeDirection = Math::Vector3::Zero;
	bool useRootMotion = false;

	std::string animationNameForward = "APose_Slide_F";
	std::string animationNameBackward = "APose_Slide_B";
	std::string animationNameLeft = "APose_Slide_L";
	std::string animationNameRight = "APose_Slide_R";

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
	bool startUseRootMotion = true;
	std::string loopAnimationName;
	std::string endAnimationName;
	float endDuration = 0.15f;
	bool endUseRootMotion = true;
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
	MotionClipData left90;
	MotionClipData right90;
	MotionClipData left180;
	MotionClipData right180;
};

// --- ガード1回分のデータ全体 -----------------------------------------
struct GuardData
{
	float justWindowDuration = 0.55f;

	std::string loopAnimationName = "DefenseL_Loop_Root";

	MotionClipData start;
	MotionClipData loop;
	MotionClipData parry;
	MotionClipData hit;
	MotionClipData end;
};

// --- チャージ攻撃の溜めデータ ----------------------------------------
struct ChargeData
{
	float maxChargeTime = 1.5f;  // この秒数で最大チャージ(以降は保持)
	float minDamageScale = 1.0f; // 溜めなしで離した時のダメージ倍率
	float maxDamageScale = 2.0f; // 最大チャージ時のダメージ倍率

	MotionClipData loop; // 溜め中の構えアニメーション

	float GetDamageScale(float elapsed) const
	{
		const float t = (maxChargeTime > 0.0f) ? std::clamp(elapsed / maxChargeTime, 0.0f, 1.0f) : 1.0f;
		return minDamageScale + (maxDamageScale - minDamageScale) * t;
	}
};

// 以前は配列サイズ(コンボ段数の上限)そのものを表す定数だったが、
// コンボ木への移行に伴い固定長配列という前提が無くなったため、
// 役割を「1回のコンボで辿った段数がこれを超えたら想定外のループと
// みなしてassertする」デバッグ用の安全弁に変更した
// (PlayerAttackTable::comboLinksの循環参照による無限ループ検出用)。
constexpr int kMaxComboChainLength = 6;