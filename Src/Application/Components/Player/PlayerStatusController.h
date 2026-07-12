#pragma once

#include "ComponentBase.h"
#include "../Objects/GameObject.h"

// ============================================================
// 移動軸。
// CombatStateがNone以外の間(攻撃/回避/怯み中)は参照されない想定。
// その間の位置移動は、各モーション側が直接扱う
// (Walk/Runの入力に頼らず、回避なら固定軌道で動く、等)。
// ============================================================
enum class MovementState
{
	Stand = 0,
	Walk,
	Run,
};

// ============================================================
// 戦闘軸。
// 「予備動作→判定中→硬直→(None or 次の行動へキャンセル)」という
// 攻撃1サイクルと、対称な回避のサイクルを持つ。
// 怯みはこのサイクルの外から割り込んでくる特別な状態として扱う。
// ============================================================
enum class CombatState
{
	None = 0,
	AttackWindup,
	AttackActive,
	AttackRecovery,
	Evade,
	EvadeRecovery,
	StaggerSmall,
	StaggerLarge,
};

// ============================================================
// 攻撃1つ分の時間的定義。技ごとにインスタンスを用意してRequestAttack()に渡す。
// ============================================================
struct AttackMoveData
{
	float windupDuration = 0.2f;
	float activeDuration = 0.15f;
	float recoveryDuration = 0.3f;

	// recoveryDuration内で、ここから先は回避によるキャンセルを許可する
	// 開始タイミング(秒)。recoveryDuration以上の値にすればキャンセル不可の技になる。
	float recoveryEvadeCancelStart = 0.15f;
};

// ============================================================
// 回避1つ分の時間的定義。
// ============================================================
struct EvadeMoveData
{
	float activeDuration = 0.25f;    // 無敵が乗っている実移動フェーズ
	float recoveryDuration = 0.15f;  // 後隙(無敵切れ)

	// activeDuration内で、ここから先が「ジャスト回避」の成立窓。
	float justWindowStart = 0.05f;
	float justWindowEnd = 0.15f;
};

// 入力バッファに積める行動の種類。今はEvadeのみだが、
// 将来コンボ継続キャンセル等を増やす場合はここに足していく。
enum class PendingActionType
{
	None = 0,
	Evade,
};

// ============================================================
// PlayerStatusController
// 移動軸/戦闘軸の2軸を1つのコンポーネントで管理する。
// 相互の制約(攻撃中はRunできない、等)を頻繁に参照し合うため、
// あえて分離せずここに集約している。
// ============================================================
class PlayerStatusController : public ComponentBase
{
public:
	explicit PlayerStatusController(GameObject* owner) : ComponentBase(owner) {}

	// --- 移動軸: 参照/要求 ------------------------------------------

	MovementState GetMovementState() const { return movementState_; }

	// 移動要求。戦闘軸がNone以外の間は無視する
	// (攻撃/回避モーション側が位置移動を専有するため)。
	void RequestMove(MovementState desired)
	{
		if (combatState_ != CombatState::None) return;
		movementState_ = desired;
	}

	// --- 戦闘軸: 参照 --------------------------------------------------

	CombatState GetCombatState() const { return combatState_; }
	float GetCombatElapsed() const { return elapsed_; }

	bool IsAttacking() const
	{
		return combatState_ == CombatState::AttackWindup
			|| combatState_ == CombatState::AttackActive
			|| combatState_ == CombatState::AttackRecovery;
	}

	bool IsEvading() const
	{
		return combatState_ == CombatState::Evade
			|| combatState_ == CombatState::EvadeRecovery;
	}

	bool IsStaggered() const
	{
		return combatState_ == CombatState::StaggerSmall
			|| combatState_ == CombatState::StaggerLarge;
	}

	// 現在、被弾判定に対して「ジャスト回避」が成立する窓の中にいるか。
	// 実際の当たり判定(衝突検出)は外部の被弾解決システムが持つため、
	// ヒット発生時にこちらへ問い合わせてもらう想定。
	bool IsInJustEvadeWindow() const
	{
		if (combatState_ != CombatState::Evade) return false;
		return elapsed_ >= currentEvade_.justWindowStart
			&& elapsed_ <= currentEvade_.justWindowEnd;
	}

	// --- 戦闘軸: 行動要求 --------------------------------------------

	// 攻撃要求。Noneの時だけ即座に開始する。
	// (硬直中からのコンボ継続キャンセルは今回のスコープ外。
	//  必要になったらRequestEvade()と同じ形でバッファに積める)
	void RequestAttack(const AttackMoveData& move)
	{
		if (combatState_ != CombatState::None) return;
		currentAttack_ = move;
		ChangeCombatState(CombatState::AttackWindup);
	}

	// 回避要求。
	// - Noneなら即座に開始
	// - AttackRecoveryでキャンセル猶予が既に開いていればそこから即座に開始
	// - それ以外(猶予未到達のAttackRecovery含む)は、ごく短い猶予
	//   (kInputBufferDuration)だけ「予約」しておき、Updateで猶予が
	//   開いた瞬間に自動消費する。バッファが短いため、Windup/Active中の
	//   入力は実質的に無効(シビア寄りの仕様)になる。
	void RequestEvade(const EvadeMoveData& move)
	{
		if (TryBeginEvadeNow(move)) return;

		pendingAction_ = PendingActionType::Evade;
		pendingEvadeMove_ = move;
		pendingBufferElapsed_ = 0.0f;
	}

	// 外部の被弾解決システムから、怯みを直接割り込ませるためのAPI。
	// Stagerは予備動作/判定/硬直のサイクルを持たず、即座に遷移する。
	void ApplyStagger(bool isLarge, float duration)
	{
		ChangeCombatState(isLarge ? CombatState::StaggerLarge : CombatState::StaggerSmall);
		staggerDuration_ = duration;
		// 怯みは全ての予約入力を握り潰す
		pendingAction_ = PendingActionType::None;
	}

	// --- ライフサイクル -------------------------------------------------

	void Update(float deltaTime) override
	{
		UpdatePendingInput(deltaTime);
		UpdateCombatState(deltaTime);
	}

private:
	// --- 内部: 回避開始可否 ----------------------------------------------

	bool CanStartEvade() const
	{
		if (combatState_ == CombatState::None) return true;
		if (combatState_ == CombatState::AttackRecovery)
		{
			return elapsed_ >= currentAttack_.recoveryEvadeCancelStart;
		}
		return false;
	}

	bool TryBeginEvadeNow(const EvadeMoveData& move)
	{
		if (!CanStartEvade()) return false;
		currentEvade_ = move;
		ChangeCombatState(CombatState::Evade);
		return true;
	}

	// --- 内部: 入力バッファ(浅め) ------------------------------------------

	void UpdatePendingInput(float deltaTime)
	{
		if (pendingAction_ == PendingActionType::None) return;

		pendingBufferElapsed_ += deltaTime;
		if (pendingBufferElapsed_ > kInputBufferDuration)
		{
			// 猶予切れ。入力は破棄する(シビア寄りの仕様)。
			pendingAction_ = PendingActionType::None;
			return;
		}

		if (pendingAction_ == PendingActionType::Evade)
		{
			if (TryBeginEvadeNow(pendingEvadeMove_))
			{
				pendingAction_ = PendingActionType::None;
			}
		}
	}

	// --- 内部: 戦闘軸の時間経過 ------------------------------------------

	void UpdateCombatState(float deltaTime)
	{
		if (combatState_ == CombatState::None) return;

		elapsed_ += deltaTime;

		switch (combatState_)
		{
		case CombatState::AttackWindup:
			if (elapsed_ >= currentAttack_.windupDuration)
			{
				ChangeCombatState(CombatState::AttackActive);
			}
			break;

		case CombatState::AttackActive:
			if (elapsed_ >= currentAttack_.activeDuration)
			{
				ChangeCombatState(CombatState::AttackRecovery);
			}
			break;

		case CombatState::AttackRecovery:
			if (elapsed_ >= currentAttack_.recoveryDuration)
			{
				ChangeCombatState(CombatState::None);
			}
			break;

		case CombatState::Evade:
			if (elapsed_ >= currentEvade_.activeDuration)
			{
				ChangeCombatState(CombatState::EvadeRecovery);
			}
			break;

		case CombatState::EvadeRecovery:
			if (elapsed_ >= currentEvade_.recoveryDuration)
			{
				ChangeCombatState(CombatState::None);
			}
			break;

		case CombatState::StaggerSmall:
		case CombatState::StaggerLarge:
			if (elapsed_ >= staggerDuration_)
			{
				ChangeCombatState(CombatState::None);
			}
			break;

		default:
			break;
		}
	}

	// 状態が実際に変わった時だけelapsed_をリセットする共通処理。
	// (Enter/Exit相当のフック差し込みが必要になったらここに足す)
	void ChangeCombatState(CombatState next)
	{
		if (next == combatState_) return;
		combatState_ = next;
		elapsed_ = 0.0f;
	}

	// --- 移動軸 -----------------------------------------------------
	MovementState movementState_ = MovementState::Stand;

	// --- 戦闘軸 -----------------------------------------------------
	CombatState combatState_ = CombatState::None;
	float elapsed_ = 0.0f;

	AttackMoveData currentAttack_;
	EvadeMoveData currentEvade_;
	float staggerDuration_ = 0.0f;

	// --- 入力バッファ(浅め) ---------------------------------------------
	// 「キャンセル猶予がまだ開いていない攻撃硬直中」に来た回避入力を、
	// ごく短時間だけ覚えておいて猶予が開いた瞬間に自動消費する。
	static constexpr float kInputBufferDuration = 0.1f;

	PendingActionType pendingAction_ = PendingActionType::None;
	EvadeMoveData pendingEvadeMove_;
	float pendingBufferElapsed_ = 0.0f;
};