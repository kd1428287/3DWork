#pragma once

#include "Application/Definitions/Effect/SlashTrailParams.h"
#include "../Common/ITextureProvider.h"
#include "SlashTrailRecorder.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル1本ぶんの「描画側」：経路(サンプル列)から頂点を作り、消去演出と描画を行う
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// サンプルの記録はComponent側のSlashTrailRecorderが行い、ここは経路のコピーを受け取るだけ。
// 頂点生成はUpdate()で行うので、カメラ更新後にDispatcher::Update()を呼べばカメラ正対が最新になる。
// 消去は頂点のLifeRateとノイズ(パーティクルと同じディゾルブ)で行い、αは焼き込まない。
//
// 【状態遷移】
//   Idle → BeginLive() → Recording(SetLivePathで経路更新) → Finish() → Dissolving → IsFinished()
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

	// パラメータ・テクスチャ・頂点バッファを用意する(定義が同じなら再呼び出し不要)
	bool Init(const SlashTrailParams& params, ITextureProvider* textureProvider);

	// 記録中(Recording)として使い始める。前回の経路・頂点は破棄する
	void BeginLive();

	// 記録中の経路を受け取る(Recording以外は無視)
	void SetLivePath(const SlashTrailRecorder& recorder);

	// 確定した経路を受け取り、一括消去(Dissolving)へ移る(Recording以外は無視)
	void Finish(const SlashTrailRecorder& recorder);

	// 毎フレーム：消去タイマー更新→頂点再構築→アップロード
	void Update(float deltaTime);

	void Draw(ParticleDrawPass pass) const;

	bool IsRecording() const { return state_ == State::Recording; }

	// 消え切った(描画できる軌跡が無い場合も含む)ら破棄・再利用してよい
	bool IsFinished() const;

	const SlashTrailParams& GetParams() const { return params_; }

	const std::vector<SlashTrailVertex>& GetVertices() const { return vertices_; }

	const std::vector<SlashTrailVertex>& GetCoreVertices() const { return coreVertices_; }

private:

	enum class State { Idle, Recording, Dissolving };

	void StartDissolve();

	// 墨の帯用テクスチャと、消去用ノイズテクスチャを解決する
	void ResolveTexture(ITextureProvider* textureProvider);

	void RebuildVertices();

	SlashTrailParams	params_;

	State	state_ = State::Idle;

	// 頂点生成の入力(記録中は未確定の先頭点つき)
	std::vector<SlashTrailSample>	path_;

	float	dissolveElapsed_ = 0.0f;

	// 記録中にSetLivePathが来ない時間(Componentが消えた場合の取りこぼし防止)
	float	liveIdleTime_ = 0.0f;

	// RebuildVertices()用の作業バッファ(毎フレームの確保を避ける)
	std::vector<SlashTrailSample>	resampled_;

	std::vector<SlashTrailVertex>	vertices_;

	std::vector<SlashTrailVertex>	coreVertices_;

	std::shared_ptr<SlashTrailRenderer>	renderer_;
	std::shared_ptr<KdTexture>				texture_;

	// 消去用ノイズ。noiseTexture_が無い場合はflags=0(ノイズ無し＝線形フェード)になる
	std::shared_ptr<KdTexture>				noiseTexture_;
	ParticleNoiseParams						noiseParams_;

	// 軌跡ごとに別の模様にする為の、ノイズUVのずらし量(BeginLiveのたびに変わる)
	DirectX::SimpleMath::Vector2			noiseOffset_ = { 0.0f, 0.0f };
};
