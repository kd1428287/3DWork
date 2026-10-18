#pragma once

#include <algorithm>
#include <functional>

// ボス撃破後のリザルト演出の進行管理。描画・入力・イベント発行は持たない
class ResultController
{
public:
	enum class Phase
	{
		Idle,		// 通常戦闘
		Slow,		// 撃破直後のスロー
		InkSpread,	// 墨が広がって白黒になる
		Show,		// リザルト文字を順に表示
		Wait,		// 入力待ち
		Exit,		// 終了(呼び出し側がシーンを進める)
	};

	struct Config
	{
		float slowTime = 1.5f;
		float spreadTime = 1.5f;
		float lineInterval = 0.6f;	// 1行出す間隔
		float lineFade = 0.4f;		// 1行のフェードイン時間
		float inputLockTime = 0.5f;	// 全表示後に入力を受けない時間
		int lineCount = 3;
	};

	// フェーズが変わった瞬間に呼ばれる
	std::function<void(Phase)> onPhaseChanged;

	void SetConfig(const Config& config) { cfg_ = config; }

	// 撃破を通知して演出を始める。2回目以降は無視
	void Start()
	{
		if (phase_ != Phase::Idle) { return; }
		ChangePhase(Phase::Slow);
	}

	// realDtはタイムスケールの影響を受けない時間、confirmは決定ボタンを押した瞬間だけtrue
	void Update(float realDt, bool confirm)
	{
		if (phase_ == Phase::Idle)
		{
			playTime_ += realDt;
			return;
		}

		time_ += realDt;
		phaseTime_ += realDt;

		switch (phase_)
		{
		case Phase::Slow:
			if (phaseTime_ >= cfg_.slowTime) { ChangePhase(Phase::InkSpread); }
			break;

		case Phase::InkSpread:
			if (phaseTime_ >= cfg_.spreadTime) { ChangePhase(Phase::Show); }
			break;

		case Phase::Show:
			// 決定で残りを一気に表示する
			if (confirm) { phaseTime_ = (std::max)(phaseTime_, AllRevealedTime()); }
			if (phaseTime_ >= AllRevealedTime()) { ChangePhase(Phase::Wait); }
			break;

		case Phase::Wait:
			if (confirm && phaseTime_ >= cfg_.inputLockTime) { ChangePhase(Phase::Exit); }
			break;

		default:
			break;
		}
	}

	Phase GetPhase() const { return phase_; }
	bool IsActive() const { return phase_ != Phase::Idle; }

	// 撃破までの経過時間(撃破後は止まる)
	float GetPlayTime() const { return playTime_; }

	// 撃破後の経過時間。水墨画のノイズ用
	float GetTime() const { return time_; }

	// 水墨画化の進行(0〜1)。最初に速く広がって減速する
	float GetInkProgress() const
	{
		switch (phase_)
		{
		case Phase::Idle:
		case Phase::Slow:
			return 0.0f;
		case Phase::InkSpread:
		{
			float t = std::clamp(phaseTime_ / cfg_.spreadTime, 0.0f, 1.0f);
			return 1.0f - (1.0f - t) * (1.0f - t);
		}
		default:
			return 1.0f;
		}
	}

	// index行目の不透明度(0〜1)
	float GetLineAlpha(int index) const
	{
		switch (phase_)
		{
		case Phase::Idle:
		case Phase::Slow:
		case Phase::InkSpread:
			return 0.0f;
		case Phase::Show:
			return std::clamp((phaseTime_ - index * cfg_.lineInterval) / cfg_.lineFade, 0.0f, 1.0f);
		default:
			return 1.0f;
		}
	}

private:
	float AllRevealedTime() const
	{
		return (cfg_.lineCount - 1) * cfg_.lineInterval + cfg_.lineFade;
	}

	void ChangePhase(Phase next)
	{
		phase_ = next;
		phaseTime_ = 0.0f;
		if (onPhaseChanged) { onPhaseChanged(next); }
	}

	Config cfg_;
	Phase phase_ = Phase::Idle;
	float phaseTime_ = 0.0f;
	float time_ = 0.0f;
	float playTime_ = 0.0f;
};
