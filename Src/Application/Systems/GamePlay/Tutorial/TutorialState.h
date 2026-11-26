#pragma once
#include <vector>
#include "Application/Definitions/Character/Common/StateMachine/StateMachine.h"
#include "TutorialContext.h"
#include "TutorialTypes.h"

// 共通のIStateに完了判定を追加。遷移はTutorialManagerだけが行う。
class ITutorialState : public IState<TutorialContext> {
public:
	virtual bool IsCompleted() const = 0;
};

// 指定位置への到達で完了
class StateReachArea : public ITutorialState {
public:
	explicit StateReachArea(const ReachAreaData& data) : data_(data) {}

	void Enter(TutorialContext* ctx) override;
	void Update(TutorialContext* ctx, float deltaTime) override;
	void Exit(TutorialContext* ctx) override;

	bool IsCompleted() const override { return isReached_; }

private:
	ReachAreaData data_;
	bool isReached_ = false;
};

// 生成した敵の全滅で完了(攻撃練習と総合練習で共用)
class StateDefeatEnemies : public ITutorialState {
public:
	explicit StateDefeatEnemies(const DefeatEnemiesData& data) : data_(data) {}

	void Enter(TutorialContext* ctx) override;
	void Update(TutorialContext* ctx, float deltaTime) override;
	void Exit(TutorialContext* ctx) override;

	bool IsCompleted() const override { return isCleared_; }

private:
	static bool IsEnemyDead(const Handle<GameObject>& enemy);

	DefeatEnemiesData data_;
	bool isCleared_ = false;
	std::vector<Handle<GameObject>> enemies_; // 生存中の敵のみ保持
};

// パリィを規定回数成功させると完了
class StateParry : public ITutorialState {
public:
	explicit StateParry(const ParryData& data) : data_(data) {}

	void Enter(TutorialContext* ctx) override;
	void Update(TutorialContext* ctx, float deltaTime) override;
	void Exit(TutorialContext* ctx) override;

	bool IsCompleted() const override { return isCompleted_; }

private:
	enum class Phase { WaitAttack, Prompt, Resolve };

	void UpdateWaitAttack(TutorialContext* ctx);
	void UpdatePrompt(TutorialContext* ctx);
	void UpdateResolve(TutorialContext* ctx);
	void NotifyParrySuccess();

	ParryData data_;
	Phase phase_ = Phase::WaitAttack;
	int successCount_ = 0;
	bool isCompleted_ = false;
	Handle<GameObject> enemy_ = Handle<GameObject>();
	ScopedSubscriber parrySuccessedSub;
};
