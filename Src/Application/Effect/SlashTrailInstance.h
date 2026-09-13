#pragma once

#include "SlashTrailParams.h"
#include <deque>
#include <vector>
#include <memory>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル(斬撃の軌跡)1本ぶんの実行時状態と、記録/更新ロジックを一元管理するクラス
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// EffectInstanceがGPUパーティクル1個分の実体だったのに対し、こちらはCPU側で
// 動的頂点バッファ相当のデータ(頂点配列)を毎フレーム組み立てる実体。
//
// 【tipOffsetの平滑化について】
//	距離ベースUV(UseDistanceBasedUV)では、直近に記録されたサンプルから現在の
//	実際の剣先までの延長距離(tipOffset)を全サンプルのUV計算に一律加算している。
//	記録中はこの延長距離が毎フレーム連続的に伸びるが、振りが速いフレームほど
//	1フレームあたりの伸び量(=UVの進行量)が大きくなり、非等幅なテクスチャの
//	模様が急激に切り替わって見える(速度が速いほど悪化する太さの脈動)。
//	Update()内でこの延長距離にEMA(TipOffsetSmoothingAlpha)を掛けて滑らかにし、
//	その平滑化済みの値(smoothedTipOffset_)をRebuildVertices()が使う
//
// 【状態遷移】
//   Idle(samples_が空・isRecording_==false)
//     ↓ BeginRecording()
//   Recording(isRecording_==true。UpdateTipBase()のたびにサンプルが増えうる)
//     ↓ EndRecording()
//   FadingOut(isRecording_==false・samples_はまだ残っている)
//     ↓ 全サンプルがFadeLengthを超えて間引かれる
//   Idle(IsFinished()==trueになる。ここで呼び出し側が破棄してよい)
//
// 【使い方(想定)】
//   SlashTrailInstance trail;
//   trail.Init(params);
//     :
//   trail.BeginRecording();                       // 攻撃開始
//   // 毎フレーム
//   trail.UpdateTipBase(tipBonePos, baseBonePos);  // 剣の現在位置を渡す
//   trail.Update(deltaTime);                       // Age更新・間引き・頂点再構築
//   trail.Draw(pass);
//     :
//   trail.EndRecording();                          // 攻撃終了(即座には消えず、フェードアウトする)
//     :
//   // 呼び出し側(ディスパッチャー相当)が毎フレーム確認する
//   if (trail.IsFinished()) { /* ここで破棄してよい */ }
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

	bool Init(const SlashTrailParams& params);

	void BeginRecording();

	// 記録中、毎フレーム剣のTip/Base座標を渡す。
	//	距離ベースUVの基準点(currentTip_)は、新規サンプルを追加するかどうかに関わらず
	//	このtipで毎フレーム更新される。新規サンプル記録時は、生の移動速度にEMAを
	//	掛けた平滑化済みの値をSlashTrailSample::Speedとして保存する
	void UpdateTipBase(const DirectX::SimpleMath::Vector3& tip, const DirectX::SimpleMath::Vector3& base);

	void EndRecording();

	// 毎フレーム呼ぶ:sinceLastSampleTime_加算→Age更新→期限切れサンプルの間引き→
	//	tipOffsetのEMA平滑化(smoothedTipOffset_の更新)→頂点配列の再構築、の順で行う
	void Update(float deltaTime);

	void Draw(ParticleDrawPass pass) const;

	bool IsRecording() const { return isRecording_; }

	bool IsFinished() const { return !isRecording_ && samples_.empty(); }

	const SlashTrailParams& GetParams() const { return params_; }

	const std::vector<SlashTrailVertex>& GetVertices() const { return vertices_; }

	const std::vector<SlashTrailVertex>& GetCoreVertices() const { return coreVertices_; }

private:

	void TrimExpiredSamples();

	void TrimOverflowSamples();

	void RebuildVertices();

	SlashTrailParams	params_;

	std::deque<SlashTrailSample>	samples_;	// 先頭=最古、末尾=最新

	bool	isRecording_ = false;

	bool							hasLastRecordedTip_ = false;
	DirectX::SimpleMath::Vector3	lastRecordedTip_ = { 0.0f, 0.0f, 0.0f };

	float	sinceLastSampleTime_ = 0.0f;

	bool							hasCurrentTip_ = false;
	DirectX::SimpleMath::Vector3	currentTip_ = { 0.0f, 0.0f, 0.0f };

	bool	hasSmoothedSpeed_ = false;
	float	smoothedSpeed_ = 0.0f;

	// 距離ベースUVのtipOffset平滑化用の状態。Update()のたびに更新される
	bool	hasSmoothedTipOffset_ = false;
	float	smoothedTipOffset_ = 0.0f;

	std::vector<SlashTrailVertex>	vertices_;

	std::vector<SlashTrailVertex>	coreVertices_;

	std::shared_ptr<SlashTrailRenderer>	renderer_;
};