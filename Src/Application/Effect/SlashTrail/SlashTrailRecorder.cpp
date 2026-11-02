#include "Application/main.h"
#include "SlashTrailRecorder.h"

namespace
{
	// これより短い間隔のサンプルでは速度を更新しない(ヒットストップ中のdt≒0で速度が跳ねるのを防ぐ)
	constexpr float kMinSpeedDt = 1.0f / 240.0f;
}

void SlashTrailRecorder::Init(const SlashTrailParams& params)
{
	params_ = params;
	Reset();
}

// 記録状態を全て初期化してIdleへ戻す
void SlashTrailRecorder::Reset()
{
	state_ = State::Idle;

	samples_.clear();

	hasCurrentTip_ = false;
	hasSmoothedSpeed_ = false;
	smoothedSpeed_ = 0.0f;
	sinceLastSampleTime_ = 0.0f;
	recordingElapsed_ = 0.0f;
}

// 記録開始：既存サンプルを破棄してゼロから記録し直す
void SlashTrailRecorder::Begin()
{
	Reset();
	state_ = State::Recording;
}

// 記録中、毎フレーム剣のTip/Base座標を渡す
void SlashTrailRecorder::UpdateTipBase(const DirectX::SimpleMath::Vector3& tip, const DirectX::SimpleMath::Vector3& base)
{
	if (state_ != State::Recording) { return; }

	currentTip_ = tip;
	currentBase_ = base;
	hasCurrentTip_ = true;

	if (!samples_.empty())
	{
		const float minDistSq = params_.Record.MinSampleDistance * params_.Record.MinSampleDistance;
		if ((tip - samples_.back().Tip).LengthSquared() < minDistSq) { return; }
	}

	AddSample(tip, base);
}

// 時間を進める：最大記録時間に達したら自動で終了する
void SlashTrailRecorder::Tick(float deltaTime)
{
	sinceLastSampleTime_ += deltaTime;

	if (state_ == State::Recording)
	{
		recordingElapsed_ += deltaTime;
		if (params_.Record.MaxRecordingTime > 0.0f && recordingElapsed_ >= params_.Record.MaxRecordingTime) { End(); }
	}
}

// 記録終了：最後の剣先を確定する
void SlashTrailRecorder::End()
{
	if (state_ != State::Recording) { return; }

	if (hasCurrentTip_ && !samples_.empty() &&
		(currentTip_ - samples_.back().Tip).LengthSquared() > 1e-10f)
	{
		AddSample(currentTip_, currentBase_);
	}

	state_ = State::Ended;
}

// 確定サンプルに、記録中のみ未確定の先頭点(現在の剣先)を足した経路を作る
void SlashTrailRecorder::BuildPath(std::vector<SlashTrailSample>& outPath) const
{
	outPath.assign(samples_.begin(), samples_.end());

	if (state_ == State::Recording && hasCurrentTip_ && !samples_.empty())
	{
		const SlashTrailSample& last = samples_.back();
		const float segLen = (currentTip_ - last.Tip).Length();
		if (segLen > 1e-5f)
		{
			SlashTrailSample head = last;
			head.Tip = currentTip_;
			head.Base = currentBase_;
			head.Dist = last.Dist + segLen;
			outPath.push_back(head);
		}
	}
}

// サンプルを1つ追加する(上限到達時は何もしない)
void SlashTrailRecorder::AddSample(const DirectX::SimpleMath::Vector3& tip, const DirectX::SimpleMath::Vector3& base)
{
	if (samples_.size() >= params_.Record.MaxSamples) { return; }

	const float dt = std::max(sinceLastSampleTime_, 1e-5f);
	const bool dtValid = (sinceLastSampleTime_ >= kMinSpeedDt);

	SlashTrailSample sample;
	sample.Tip = tip;
	sample.Base = base;

	float rawSpeed = params_.Width.SpeedReference;
	if (!samples_.empty())
	{
		const float segLen = (tip - samples_.back().Tip).Length();
		sample.Dist = samples_.back().Dist + segLen;

		if (dtValid) { rawSpeed = segLen / dt; }
		else if (hasSmoothedSpeed_) { rawSpeed = smoothedSpeed_; }
	}

	if (hasSmoothedSpeed_)
	{
		const float tau = params_.Record.SpeedSmoothingTime;
		const float alpha = (tau > 1e-5f) ? 1.0f - expf(-dt / tau) : 1.0f;
		smoothedSpeed_ = std::lerp(smoothedSpeed_, rawSpeed, alpha);
	}
	else
	{
		smoothedSpeed_ = rawSpeed;
		hasSmoothedSpeed_ = true;
	}

	sample.Speed = smoothedSpeed_;
	samples_.push_back(sample);

	sinceLastSampleTime_ = 0.0f;
}
