#pragma once

#include "../Combat/CombatData.h"

// ============================================================
// PlayerStatusController / PlayerInputComponent 双方から参照される
// 型をまとめた共有ヘッダ。
//
// 【AttackData(CombatData.h)への統合について】
// 以前はここにPlayer専用のAttackMoveData(windup/active/recoveryの秒数、
// ステップ移動、キャンセル受付、武器スロット等を1つに束ねたもの)を
// 独自定義していたが、Enemy側のEnemyAttackDefinition(EnemyAIData.h)と
// 概念が重複していたため、汎用のAttackData(CombatData.h)へ統合した。
// Player用の1技分のデータは以降AttackData型そのものを使う
// (info/moveData/phaseData/cancelData/weaponSlotsの構成はCombatData.h
// 参照)。
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

// --- 単発アニメーション1本分の再生設定 -----------------------------
// windup/active/recoveryのような複数フェーズへの分割は持たず、
// 「1つのクリップをどう再生するか」だけを表す小さなデータ。
// (旧名ActionPhaseData。実際には複数フェーズに分かれておらず、
// 名前の「Phase」が実態と合っていなかったため改名した。)
struct MotionClipData
{
	float duration = 0.2f;
	std::string animationName;
	bool useRootMotion = false;
	float blendDuration = 0.1f;
};

// --- Player用の1コンボ攻撃1発分のデータ ---------------------------
// 旧AttackMoveData(このファイル内で独自定義していたもの)はAttackData
// (CombatData.h)へ統合されたため、Playerの1技分の設定は今後AttackData
// 型をそのまま使う。フィールドの対応は以下の通り:
//
//   旧 AttackMoveData::windupDuration            → AttackData::phaseData.windup.targetDuration
//   旧 AttackMoveData::activeDuration             → AttackData::phaseData.active.targetDuration
//   旧 AttackMoveData::recoveryDuration            → AttackData::phaseData.recovery.targetDuration
//   旧 AttackMoveData::animationName               → AttackData::phaseData.animationName
//   旧 AttackMoveData::stepDistance/stepDuration/  → AttackData::moveData.stepDistance/stepDuration/
//       engageDistance/stepDirection/blendDuration/   engageDistance/stepDirection/blendDuration/
//       useRootMotion                                 useRootMotion
//   旧 AttackMoveData::recoveryEvadeCancelStart/   → AttackData::cancelData.recoveryEvadeCancelStart/
//       recoveryAttackCancelStart/                     recoveryAttackCancelStart/
//       comboWindowAfterRecovery                       comboWindowAfterRecovery
//   旧 AttackMoveData::weaponSlots                  → AttackData::weaponSlots
//
// ダメージ・体幹ダメージ等(AttackData::info)は旧構造体には無かった値で、
// 統合にあたり新たに保持できるようになった(既定値のままでよければ
// 変更不要)。AttackData::cancelData.recoveryMoveCancelStartもPlayer側では
// 未使用だった項目だが、既定値のまま無視して問題ない。
//
// PlayerStatusController等、旧AttackMoveData型で1技分のテーブルを
// 保持していた箇所は、型をAttackDataに置き換えた上で上記対応表に
// 沿ってメンバ参照を書き換えること。

// --- コンボの繋がり方(コンボ木) -------------------------------------
// 以前はコンボを「配列＋comboIndex_をインクリメント」だけで表現しており、
// 「次の攻撃は常にindex+1」「全攻撃がコンボに参加できる」という前提が
// 暗黙に組み込まれていた。分岐コンボや、単発で完結する非コンボ攻撃
// (強攻撃・突進攻撃等)を追加できるよう、攻撃をID参照で繋ぐ木構造へ
// 置き換える。

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
	MotionClipData left90;
	MotionClipData right90;
	MotionClipData left180;
	MotionClipData right180;
};

// --- ガード1回分のデータ全体 -----------------------------------------
// (旧名GuardMoveData。ガード自体はその場に留まる行動で移動を伴わない
// ため、「Move」という名前が実態と合っていなかった。AttackData/
// EvadeDataと同じ「〜Data」の命名に揃えた。)
struct GuardData
{
	float justWindowDuration = 0.55f;

	// ガードへ入る際に一度だけ再生する構え動作。この秒数(startDuration)が
	// 経過したらloopAnimationNameへ切り替える(StateGuard参照)。
	std::string animationName = "APose2DefenseL";
	float startDuration = 0.2f;

	// ガード継続姿勢。以前は構え動作(animationName)の単発再生を最終フレームで
	// 止めることで継続姿勢を表現していたが、その方式だと別の単発アニメーション
	// (ガードヒット等)を一度挟んだ後、同じ構え動作を再度指定しても
	// 「既に同じアニメーションが設定済み」と判定され再生されなくなる問題が
	// あった。そのため継続姿勢は専用のLoopアニメーションとして持たせ、
	// 構え動作終了後・各種リアクション終了後は常にこちらへ明示的に
	// 再生し直す。
	std::string loopAnimationName = "DefenseL_Loop";

	// パリィ成立時に再生する専用モーションと、その再生を強制する秒数
	// (この間はガードキーを離しても解除されない。StateGuard参照)。
	std::string parrySuccessAnimationName = "DefenseL_Parry01";
	float parrySuccessDuration = 0.8f;

	// 通常ブロックで被弾する都度再生するヒットリアクションモーション。
	// パリィ成功と異なり、ガード解除や反撃キャンセルの可否には影響しない。
	std::string guardHitAnimationName = "DefenseL_Hit01";
	float guardHitDuration = 0.9f;
};

// 以前は配列サイズ(コンボ段数の上限)そのものを表す定数だったが、
// コンボ木への移行に伴い固定長配列という前提が無くなったため、
// 役割を「1回のコンボで辿った段数がこれを超えたら想定外のループと
// みなしてassertする」デバッグ用の安全弁に変更した
// (PlayerAttackTable::comboLinksの循環参照による無限ループ検出用)。
constexpr int kMaxComboChainLength = 6;