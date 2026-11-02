#pragma once

#include "Application/Definitions/Effect/SlashTrailParams.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル(斬撃の軌跡)1本ぶんの「記録」だけを担当する
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 剣のTip/Baseを間引いてサンプル化し、距離・平滑化済み速度を持たせる。
// D3Dにも描画にも依存しない(頂点生成・消去演出はSlashTrailInstance側)。
// 時間はTick(dt)で進めるので、渡すdtがそのまま記録の時間軸になる(ゲーム時間を渡す)。
//
// 【状態遷移】
//   Idle → Begin() → Recording → End()/最大記録時間 → Ended → Reset()/Begin() → ...
//
// 【使い方】
//   rec.Init(params);
//   rec.Begin();
//   rec.UpdateTipBase(tip, base);   // 毎フレーム
//   rec.Tick(dt);                   // 毎フレーム
//   rec.BuildPath(path);            // 頂点生成の入力(記録中は未確定の先頭点つき)
//   rec.End();
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class SlashTrailRecorder
{
public:

	SlashTrailRecorder() {}

	// 記録に必要なパラメータを丸ごと保持する(後からComponent間でmoveしても自己完結させる為)
	void Init(const SlashTrailParams& params);

	// 全ての記録を破棄してIdleへ戻す
	void Reset();

	// Reset()してRecordingへ入る
	void Begin();

	// 記録中、毎フレーム剣のTip/Base座標を渡す(Recording以外は無視)
	void UpdateTipBase(const DirectX::SimpleMath::Vector3& tip, const DirectX::SimpleMath::Vector3& base);

	// 時間を進める。最大記録時間に達したらEnd()する
	void Tick(float deltaTime);

	// 最後の剣先を確定してEndedへ移る(Recording以外は無視)
	void End();

	bool IsRecording() const { return state_ == State::Recording; }

	const std::vector<SlashTrailSample>& GetSamples() const { return samples_; }

	// 確定サンプル+(記録中のみ)現在の剣先に追従する未確定の先頭点を出力する
	void BuildPath(std::vector<SlashTrailSample>& outPath) const;

private:

	enum class State { Idle, Recording, Ended };

	void AddSample(const DirectX::SimpleMath::Vector3& tip, const DirectX::SimpleMath::Vector3& base);

	SlashTrailParams	params_;

	State	state_ = State::Idle;

	std::vector<SlashTrailSample>	samples_;	// 先頭=始点、末尾=最新

	bool							hasCurrentTip_ = false;
	DirectX::SimpleMath::Vector3	currentTip_ = { 0.0f, 0.0f, 0.0f };
	DirectX::SimpleMath::Vector3	currentBase_ = { 0.0f, 0.0f, 0.0f };

	float	sinceLastSampleTime_ = 0.0f;

	bool	hasSmoothedSpeed_ = false;
	float	smoothedSpeed_ = 0.0f;

	float	recordingElapsed_ = 0.0f;
};
