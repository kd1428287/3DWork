#pragma once
#include <memory>
#include <vector>
#include "IBTNode.h"

// ============================================================
// 子を上から順に実行し、途中でFailureが出たら即Failureで打ち切る
// (全員Successならこのノード自体もSuccess)。
// 「A→B→Cを順番にやり切って初めて成功」という直列的な行動列を
// 表したい時に使う(例: 索敵成功→接近成功→攻撃成功)。
//
// こちらはresumption方式のまま(前回Runningだった子から再開する)。
// Sequenceは「一度Successした子を後で再チェックしない」という契約
// (前の子がSuccessした前提で次に進む)なので、Selectorと違って
// 毎フレーム先頭からの再評価は不要(BTSelectorのコメント参照)。
// ============================================================
template <typename T>
class BTSequence : public IBTNode<T>
{
public:
	void AddChild(std::unique_ptr<IBTNode<T>> child) { children_.push_back(std::move(child)); }

	BTNodeStatus Tick(T* context, float deltaTime) override
	{
		// 前回Runningだった子から再開する。毎回先頭から評価し直すと、
		// 時間のかかるActionノードが持つ内部状態を保持している意味が
		// 無くなってしまうため。
		while (runningIndex_ < children_.size()) {
			const BTNodeStatus status = children_[runningIndex_]->Tick(context, deltaTime);
			if (status == BTNodeStatus::Running) return BTNodeStatus::Running;
			if (status == BTNodeStatus::Failure) {
				runningIndex_ = 0;
				return BTNodeStatus::Failure;
			}
			++runningIndex_; // Successなら次の子へ
		}
		runningIndex_ = 0;
		return BTNodeStatus::Success;
	}

	void Reset() override
	{
		if (runningIndex_ < children_.size()) children_[runningIndex_]->Reset();
		runningIndex_ = 0;
	}

private:
	std::vector<std::unique_ptr<IBTNode<T>>> children_;
	size_t runningIndex_ = 0;
};

// ============================================================
// 子を上から順に試し、最初にSuccess(またはRunning)を返した子で打ち切る
// (全員Failureならこのノード自体もFailure)。
// 「優先度の高い行動から順に試して、できるものを1つ選ぶ」用途に使う
// (例: 攻撃可能ならAttack、無理ならChase、それも無理ならIdle)。
//
// 【reactive方式】毎フレーム必ず先頭(優先度0)の子から評価し直す。
// resumption方式(前回Runningだった子から再開する)だと、一度低優先度の
// 行動(Chase等)にRunningで居座ってしまうと、その行動が自力でFailure/
// Successを返すまで、より優先度の高い行動(Attack等)が実行可能に
// なったことに気づけない。Sekiro/NieR系のように「プレイヤーの動きに
// 機敏に反応してほしい」敵AIにはこの反応の遅れが致命的なため、
// 毎フレーム全候補を先頭から洗い直すreactive方式を採用する。
//
// 代償として、既にRunningな行動があっても毎フレーム自分より優先度の
// 高い候補(Condition等)を評価し直すコストが発生する。Conditionノードは
// 通常「距離を1回比較するだけ」のような軽い処理である前提でこの
// コストを許容している。もしCondition側が重くなってきたら、
// 「N フレームに1回だけ上位の再評価を行う」等のスロットリングを
// このTick()に足すことを検討すること。
// ============================================================
template <typename T>
class BTSelector : public IBTNode<T>
{
public:
	void AddChild(std::unique_ptr<IBTNode<T>> child) { children_.push_back(std::move(child)); }

	BTNodeStatus Tick(T* context, float deltaTime) override
	{
		for (size_t i = 0; i < children_.size(); ++i) {
			const BTNodeStatus status = children_[i]->Tick(context, deltaTime);

			if (status == BTNodeStatus::Running || status == BTNodeStatus::Success) {
				// 直前まで別の子がRunningだった場合、その子を「優先度の
				// 高い子に割り込まれて中断された」ものとして後始末させる
				// (EnemyActionIdle::Reset()がisPlaying_をfalseに戻すのと
				//  同じ役割。中断された子が再び選ばれた時、Enter()相当の
				//  初期化を確実にやり直させるために必要)。
				if (hasRunning_ && runningIndex_ != i) {
					children_[runningIndex_]->Reset();
				}
				hasRunning_ = (status == BTNodeStatus::Running);
				runningIndex_ = i;
				return status;
			}
			// Failureなら次の子を同じフレーム内で続けて試す。
		}

		// 全員Failure。直前までRunningだった子がいれば、それも
		// 実行不可能になったとみなして後始末する。
		if (hasRunning_) {
			children_[runningIndex_]->Reset();
			hasRunning_ = false;
		}
		return BTNodeStatus::Failure;
	}

	void Reset() override
	{
		if (hasRunning_) {
			children_[runningIndex_]->Reset();
			hasRunning_ = false;
		}
	}

private:
	std::vector<std::unique_ptr<IBTNode<T>>> children_;
	size_t runningIndex_ = 0;

	// runningIndex_だけでは「0番目の子がRunning中」と「まだ誰も
	// Running/Successを返したことがない(初期状態)」を区別できない
	// (どちらもrunningIndex_==0になりうる)。この区別を誤ると、
	// 初回Tick時に実行してもいない子へ誤ってReset()を呼んでしまう
	// (Reset()の実装がべき等でない場合に不具合の原因になりうる)ため、
	// 「現在Running中の子が存在するか」を明示的にこのフラグで持つ。
	bool hasRunning_ = false;
};
