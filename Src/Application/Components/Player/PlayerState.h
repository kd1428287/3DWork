#pragma once
#include "PlayerCombatTypes.h"

// 前方宣言
class PlayerStatusController;

// ============================================================
// IPlayerState
// 軽量Stateパターンの基底インターフェース。GameObjectにはアタッチせず、
// PlayerStatusController内部でインスタンスとして保持される。
// ============================================================
class IPlayerState {
public:
	virtual ~IPlayerState() = default;

	// 状態のライフサイクル
	virtual void Enter(PlayerStatusController* controller) {}
	virtual void Update(PlayerStatusController* controller, float deltaTime) {}
	virtual void Exit(PlayerStatusController* controller) {}

	// 現在の具体的なフェーズと経過時間を返す
	virtual CombatState GetDetailedState() const = 0;
	virtual float GetElapsed() const = 0;

	// 次のアクションへの遷移可否（デフォルトは不可）
	virtual bool CanStartAttack(const PlayerStatusController* controller) const { return false; }
	virtual bool CanStartEvade(const PlayerStatusController* controller) const { return false; }

	// ジャスト判定の問い合わせ（該当するStateのみがオーバーライドしてtrueを返す）
	virtual bool IsInJustEvadeWindow(const PlayerStatusController* controller) const { return false; }
	virtual bool IsInParryWindow(const PlayerStatusController* controller) const { return false; }
};


class StateNone : public IPlayerState {
public:
	CombatState GetDetailedState() const override { return CombatState::None; }
	float GetElapsed() const override { return 0.0f; }

	// None状態からは何でも出せる
	bool CanStartAttack(const PlayerStatusController* controller) const override { return true; }
	bool CanStartEvade(const PlayerStatusController* controller) const override { return true; }
};


class StateAttack : public IPlayerState {
public:
	void Enter(PlayerStatusController* controller) override;
	void Update(PlayerStatusController* controller, float deltaTime) override;

	CombatState GetDetailedState() const override { return phase_; }
	float GetElapsed() const override { return elapsed_; }

	bool CanStartEvade(const PlayerStatusController* controller) const override;

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

private:
	float elapsed_ = 0.0f;
};


class StateStagger : public IPlayerState {
public:
	void Setup(bool isLarge, float duration) {
		isLarge_ = isLarge;
		duration_ = duration;
	}

	void Enter(PlayerStatusController* controller) override;
	void Update(PlayerStatusController* controller, float deltaTime) override;

	CombatState GetDetailedState() const override {
		return isLarge_ ? CombatState::StaggerLarge : CombatState::StaggerSmall;
	}
	float GetElapsed() const override { return elapsed_; }

private:
	bool isLarge_ = false;
	float duration_ = 0.0f;
	float elapsed_ = 0.0f;
};