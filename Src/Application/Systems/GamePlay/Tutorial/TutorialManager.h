#pragma once
#include <cstddef>
#include <memory>
#include <vector>
#include "TutorialState.h"

// チュートリアルの順番と遷移を管理する。TutorialSceneがメンバとして持つ。
class TutorialManager {
public:
	void Initialize(const TutorialContext& context, const TutorialConfig& config);
	void Update(float deltaTime);
	void Finalize(); // シーンの依存が破棄される前に呼ぶ

	void Skip() { Finish(); }
	bool IsFinished() const { return isFinished_; }

private:
	void BuildStates(const TutorialConfig& config);
	void ChangeState(std::size_t index);
	void Finish();

	TutorialContext ctx_;
	std::vector<std::unique_ptr<ITutorialState>> states_;
	std::size_t currentIndex_ = 0;
	bool isFinished_ = false;
	StateMachine<TutorialContext, ITutorialState> stateMachine_;
};
