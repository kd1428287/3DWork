#include "TutorialManager.h"

void TutorialManager::Initialize(const TutorialContext& context, const TutorialConfig& config)
{
	Finalize();
	ctx_ = context;
	isFinished_ = false;
	// TODO: プレイヤーのHP下限を1に設定する
	BuildStates(config);
	ChangeState(0);
}

void TutorialManager::Update(float deltaTime)
{
	ITutorialState* current = stateMachine_.Current();
	if (current == nullptr) return;

	stateMachine_.Update(&ctx_, deltaTime);
	// TODO: 完了から次へ進むまでの待ち時間を入れる
	if (current->IsCompleted()) ChangeState(currentIndex_ + 1);
}

void TutorialManager::Finalize()
{
	stateMachine_.TransitionTo(&ctx_, nullptr); // 現在のStateのExitを呼ぶ
	states_.clear();
}

void TutorialManager::BuildStates(const TutorialConfig& config)
{
	states_.push_back(std::make_unique<StateReachArea>(config.move));
	states_.push_back(std::make_unique<StateDefeatEnemies>(config.attack));
	states_.push_back(std::make_unique<StateParry>(config.parry));
	states_.push_back(std::make_unique<StateDefeatEnemies>(config.finalBattle));
}

void TutorialManager::ChangeState(std::size_t index)
{
	if (index >= states_.size()) {
		Finish();
		return;
	}
	currentIndex_ = index;
	stateMachine_.TransitionTo(&ctx_, states_[currentIndex_].get());
}

void TutorialManager::Finish()
{
	if (isFinished_) return; // 遷移要求の多重発行を防ぐ
	isFinished_ = true;
	stateMachine_.TransitionTo(&ctx_, nullptr);
	// TODO: ボス戦シーンへのSceneChangeRequestを発行する
}
