#pragma once
#include "PlayerCombatTypes.h"
#include "../StateMachine/StateMachine.h"

// 前方宣言
class PlayerStatusController;

// ============================================================
// IPlayerState
// 共通StateMachine基盤(IState<PlayerStatusController>)に、
// プレイヤー戦闘特有の問い合わせ(具体フェーズ・遷移可否・ジャスト判定)を
// 追加した軽量Stateパターンのインターフェース。GameObjectにはアタッチせず、
// PlayerStatusController内部でインスタンスとして保持される。
// ============================================================
class IPlayerState : public IState<PlayerStatusController> {
public:
	// 現在の具体的なフェーズと経過時間を返す
	virtual CombatState GetDetailedState() const = 0;
	virtual float GetElapsed() const = 0;

	// 次のアクションへの遷移可否（デフォルトは不可）。
	// Guardもここに揃えることで、HandleActionInput側にCombatStateの
	// ハードコード判定を書かずに済むようにする。
	virtual bool CanStartAttack(const PlayerStatusController* controller) const { return false; }
	virtual bool CanStartEvade(const PlayerStatusController* controller) const { return false; }
	virtual bool CanStartGuard(const PlayerStatusController* controller) const { return false; }

	// ガードキーを離した際に即座に解除してよいか(デフォルトは常に許可)。
	// StateGuardがパリィ成功演出中だけfalseを返し、演出を強制的に
	// 見せ切る(HandleActionInput参照)。
	virtual bool CanReleaseGuard(const PlayerStatusController* controller) const { return true; }

	// ジャスト判定の問い合わせ（該当するStateのみがオーバーライドしてtrueを返す）
	virtual bool IsInvincible(const PlayerStatusController* controller) const { return false; }
	virtual bool IsInJustEvadeWindow(const PlayerStatusController* controller) const { return false; }
	virtual bool IsInParryWindow(const PlayerStatusController* controller) const { return false; }
};

class StateNone : public IPlayerState {
public:
	// Noneに戻った瞬間、現在の入力状態に合わせてIdle/Walk/Runへ
	// アニメーションを同期し直す(行動中ずっと同じ入力のままだった場合の
	// 固まり対策。詳細はPlayerStatusController::RefreshMovementAnimation参照)。
	void Enter(PlayerStatusController* controller) override;

	CombatState GetDetailedState() const override { return CombatState::None; }
	float GetElapsed() const override { return 0.0f; }

	// None状態からは何でも出せる
	bool CanStartAttack(const PlayerStatusController* controller) const override { return true; }
	bool CanStartEvade(const PlayerStatusController* controller) const override { return true; }
	bool CanStartGuard(const PlayerStatusController* controller) const override { return true; }
};

class StateAttack : public IPlayerState {
public:
	void Enter(PlayerStatusController* controller) override;
	void Update(PlayerStatusController* controller, float deltaTime) override;
	void Exit(PlayerStatusController* controller) override;

	CombatState GetDetailedState() const override { return phase_; }
	float GetElapsed() const override { return elapsed_; }

	// Recovery中の一定タイミングを過ぎたら、回避だけでなく次の攻撃
	// (コンボ)によるキャンセルも許可する。
	bool CanStartAttack(const PlayerStatusController* controller) const override;
	bool CanStartEvade(const PlayerStatusController* controller) const override;
	bool CanStartGuard(const PlayerStatusController* controller) const override;

private:
	CombatState phase_ = CombatState::AttackWindup;
	float elapsed_ = 0.0f;
};


class StateEvade : public IPlayerState {
public:
	void Enter(PlayerStatusController* controller) override;
	void Update(PlayerStatusController* controller, float deltaTime) override;
	void Exit(PlayerStatusController* controller) override;

	CombatState GetDetailedState() const override { return phase_; }
	float GetElapsed() const override { return elapsed_; }

	bool IsInJustEvadeWindow(const PlayerStatusController* controller) const override;
	bool IsInvincible(const PlayerStatusController* controller) const override; // 追加

private:
	CombatState phase_ = CombatState::Evade;
	float elapsed_ = 0.0f;
};


class StateGuard : public IPlayerState {
public:
	void Enter(PlayerStatusController* controller) override;
	void Update(PlayerStatusController* controller, float deltaTime) override;

	CombatState GetDetailedState() const override { return CombatState::Guard; }
	float GetElapsed() const override { return elapsed_; }

	bool IsInParryWindow(const PlayerStatusController* controller) const override;

	// パリィ成功演出中はガードキーを離しても解除させない(強制的に見せ切る)。
	bool CanReleaseGuard(const PlayerStatusController* controller) const override;

	// パリィ成功演出中だけ、反撃キャンセルとして次の攻撃を許可する。
	bool CanStartAttack(const PlayerStatusController* controller) const override;

	//bool CanStartEvade(const PlayerStatusController* controller) const override { return true; }

	// HitReactionComponent(IHitReactionQuery経由、PlayerStatusController::
	// NotifyParrySuccess/NotifyGuardHit)から呼ばれる通知。
	void NotifyParrySuccess(PlayerStatusController* controller);
	void NotifyGuardHit(PlayerStatusController* controller);

private:
	// パリィ判定はGuard内部のサブフェーズとして扱う。
	// ParrySuccessは時間経過ではなくNotifyParrySuccess()による外部通知で
	// 開始し、parrySuccessDuration秒経過で自動的にNormalBlockへ戻る
	// (GetGuardPhase()参照。CombatStateやControllerには手を入れず
	//  Guard内部だけで完結させる、という元々の方針を踏襲)。
	enum class GuardPhase { JustWindow, NormalBlock, ParrySuccess };
	GuardPhase GetGuardPhase(const PlayerStatusController* controller) const;

	float elapsed_ = 0.0f;
	bool parrySucceeded_ = false;
	float parrySuccessElapsed_ = 0.0f;
};


class StateStagger : public IPlayerState {
public:
	void Setup(bool isLarge, float duration) {
		isLarge_ = isLarge;
		duration_ = duration;
	}

	void Enter(PlayerStatusController* controller) override;
	void Update(PlayerStatusController* controller, float deltaTime) override;
	void Exit(PlayerStatusController* controller) override;

	CombatState GetDetailedState() const override {
		return isLarge_ ? CombatState::StaggerLarge : CombatState::StaggerSmall;
	}
	float GetElapsed() const override { return elapsed_; }

private:
	bool isLarge_ = false;
	float duration_ = 0.0f;
	float elapsed_ = 0.0f;
};