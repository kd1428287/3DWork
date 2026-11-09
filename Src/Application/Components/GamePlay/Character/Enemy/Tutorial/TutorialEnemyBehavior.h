#pragma once
#include <string>
#include <vector>
#include "Application/Definitions/Character/Enemy/IEnemyBehavior.h"

// フェーズ中の敵の振る舞い。Idle=棒立ち、Scripted=指定技を使う、Free=通常の抽選戦闘。
enum class TutorialPhaseMode { Idle, Scripted, Free };

struct TutorialPhase
{
	TutorialPhaseMode mode = TutorialPhaseMode::Idle;
	std::string attackId;	// Scripted時に使う技ID(EnemyAIData::attacksのid)
	bool repeat = false;	// falseなら1回撃ったあとは待機。trueならクールダウンごとに繰り返す
};

// ============================================================
// チュートリアル用の敵の判断層。進行度マネージャーが
// Blackboard(kPhaseKey)へフェーズ番号を書き込み、その番号で振る舞いを切り替える。
// 例: controller->GetBlackboard().SetFloat(TutorialEnemyBehavior::kPhaseKey, 2.0f);
// ============================================================
class TutorialEnemyBehavior : public IEnemyBehavior
{
public:
	static constexpr const char* kPhaseKey = "TutorialPhase";

	explicit TutorialEnemyBehavior(std::vector<TutorialPhase> phases = DefaultPhases())
		: phases_(std::move(phases)) {
	}

	std::unique_ptr<IBTNode<EnemyAIController>> BuildTree(EnemyAIController* owner) override;
	void OnStaggered(EnemyAIController* owner, bool isLarge, float duration) override;
	void OnParrySucceeded(EnemyAIController* owner, const Events::ParrySucceeded& event) override;
	void OnDied(EnemyAIController* owner) override;
	float GetDespawnDelay(const EnemyAIController* owner) const override;

	static std::vector<TutorialPhase> DefaultPhases();

private:
	// 数値が小さいほど優先度が高い。
	enum class ReactionPriority : int { BigStagger = 0, Parried = 1, HitReaction = 2 };

	struct PendingReaction {
		std::string oneShotAnimationKey; // 空なら要求無し
		ReactionPriority priority = ReactionPriority::HitReaction;
	};

	void RequestReaction(EnemyAIController* owner, const std::string& oneShotAnimationKey, ReactionPriority priority);

	// 現在のフェーズを返す。番号が変わったら消化済みフラグを戻す。
	const TutorialPhase& CurrentPhase(EnemyAIController* c);
	bool IsMode(EnemyAIController* c, TutorialPhaseMode mode) { return CurrentPhase(c).mode == mode; }
	// Scriptedで、まだ撃つ権利が残っているか。
	bool IsScriptedPending(EnemyAIController* c);
	const EnemyAttackDefinition* ScriptedAttack(EnemyAIController* c);
	bool IsInScriptedRange(EnemyAIController* c);

	std::vector<TutorialPhase> phases_;
	int lastPhaseIndex_ = -1;
	bool consumed_ = false; // 現フェーズでScripted技を既に選んだか

	PendingReaction pending_;
};
