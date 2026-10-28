#pragma once

#include "Application/Definitions/Effect/SlashTrailParams.h"
#include "../Common/ITextureProvider.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル(斬撃の軌跡)1本ぶんの記録と頂点生成
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 一筆として記録し、記録終了後は後ろから縮ませず、全体を一括で消す。
// UVは「始点からの距離 ÷ StrokeLength」(0～1にclamp)で、記録済みサンプルのUVは後から変わらない。
//
// 【状態遷移】
//   Idle → BeginRecording() → Recording → EndRecording() → Dissolving → IsFinished() → Idle扱い
//
// 【使い方】
//   trail.Init(params, textureProvider);
//   trail.BeginRecording();
//   trail.UpdateTipBase(tip, base);   // 毎フレーム
//   trail.Update(deltaTime);
//   trail.Draw(pass);
//   trail.EndRecording();             // 以後はDissolveTimeかけて全体が消える
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class SlashTrailInstance
{
public:

	SlashTrailInstance() {}
	~SlashTrailInstance() {}

	SlashTrailInstance(const SlashTrailInstance&) = delete;
	SlashTrailInstance& operator=(const SlashTrailInstance&) = delete;

	SlashTrailInstance(SlashTrailInstance&&) = default;
	SlashTrailInstance& operator=(SlashTrailInstance&&) = default;

	bool Init(const SlashTrailParams& params, ITextureProvider* textureProvider);

	void BeginRecording();

	// 記録中、毎フレーム剣のTip/Base座標を渡す
	void UpdateTipBase(const DirectX::SimpleMath::Vector3& tip, const DirectX::SimpleMath::Vector3& base);

	// 最後の剣先を確定してDissolvingへ移る
	void EndRecording();

	// 毎フレーム呼ぶ：消去タイマー更新→頂点再構築
	void Update(float deltaTime);

	void Draw(ParticleDrawPass pass) const;

	bool IsRecording() const { return state_ == State::Recording; }

	// 消え切った(または一度も描画できる軌跡が無かった)ら呼び出し側が破棄してよい
	bool IsFinished() const;

	const SlashTrailParams& GetParams() const { return params_; }

	const std::vector<SlashTrailVertex>& GetVertices() const { return vertices_; }

	const std::vector<SlashTrailVertex>& GetCoreVertices() const { return coreVertices_; }

private:

	enum class State { Idle, Recording, Dissolving };

	void ResetState();

	void ResolveTexture(ITextureProvider* textureProvider);

	void AddSample(const DirectX::SimpleMath::Vector3& tip, const DirectX::SimpleMath::Vector3& base);

	void RebuildVertices();

	SlashTrailParams	params_;

	State	state_ = State::Idle;

	std::vector<SlashTrailSample>	samples_;	// 先頭=始点、末尾=最新

	bool							hasCurrentTip_ = false;
	DirectX::SimpleMath::Vector3	currentTip_ = { 0.0f, 0.0f, 0.0f };
	DirectX::SimpleMath::Vector3	currentBase_ = { 0.0f, 0.0f, 0.0f };

	float	sinceLastSampleTime_ = 0.0f;

	bool	hasSmoothedSpeed_ = false;
	float	smoothedSpeed_ = 0.0f;

	float	dissolveElapsed_ = 0.0f;
	float	recordingElapsed_ = 0.0f;

	// RebuildVertices()用の作業バッファ(毎フレームの確保を避ける)
	std::vector<SlashTrailSample>	path_;
	std::vector<SlashTrailSample>	resampled_;

	std::vector<SlashTrailVertex>	vertices_;

	std::vector<SlashTrailVertex>	coreVertices_;

	std::shared_ptr<SlashTrailRenderer>	renderer_;
	std::shared_ptr<KdTexture>				texture_;
};
