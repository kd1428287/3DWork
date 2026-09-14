#pragma once

// ============================================================
// IState<TController> / StateMachine<TController, TState>
//
// PlayerStatusController / EnemyStatusController に共通する
// 「軽量Stateパターンの基盤(遷移ロジックとUpdateの丸投げ)」だけを
// テンプレートで切り出したもの。
//
// 各コントローラ固有の問い合わせ(CombatStateやジャスト判定など)は
// ここには置かず、TController側のIXxxState(IPlayerState/IEnemyStateなど)
// でIStateを継承して追加する。「仕組みは共通・中身は別」という方針。
// ============================================================

template<typename TController>
class IState {
public:
	virtual ~IState() = default;

	virtual void Enter(TController* controller) {}
	virtual void Update(TController* controller, float deltaTime) {}
	virtual void Exit(TController* controller) {}
};

template<typename TController, typename TState = IState<TController>>
class StateMachine {
public:
	using StatePtr = TState*;

	// 現在のStateへdeltaTimeを渡して更新する
	void Update(TController* controller, float deltaTime) {
		if (currentState_ != nullptr) {
			currentState_->Update(controller, deltaTime);
		}
	}

	// 同一Stateへの遷移は無視する。実際に遷移が起きた場合のみtrueを返す
	// (呼び出し側が「遷移した時だけ追加処理をしたい」ケースで使える)。
	bool TransitionTo(TController* controller, StatePtr nextState) {
		if (currentState_ == nextState) return false;
		ForceTransitionTo(controller, nextState);
		return true;
	}

	// 同じStateインスタンスへの遷移でも必ずExit→Enterし直す版。
	// 例: 怯み中にさらにヒットして怯みが再発生する場合など、
	// 内部タイマーをリセットしたいケースで使う。
	void ForceTransitionTo(TController* controller, StatePtr nextState) {
		if (currentState_ != nullptr) currentState_->Exit(controller);
		currentState_ = nextState;
		if (currentState_ != nullptr) currentState_->Enter(controller);
	}

	StatePtr Current() const { return currentState_; }

private:
	StatePtr currentState_ = nullptr;
};
