#include "../main.h"
#include "SlashTrailInstance.h"

#include <algorithm>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 初期化：パラメータを保持し、状態を初期化する(GPU側の資源確保は未実装)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool SlashTrailInstance::Init(const SlashTrailParams& params)
{
	params_ = params;

	samples_.clear();
	vertices_.clear();

	isRecording_ = false;
	hasLastRecordedTip_ = false;

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 記録開始：既存サンプルを破棄してゼロから記録し直す
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::BeginRecording()
{
	samples_.clear();
	vertices_.clear();

	isRecording_ = true;
	hasLastRecordedTip_ = false;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 記録中、毎フレーム剣のTip/Base座標を渡す
//	距離ベースの間引き：前回記録したTipからMinSampleDistance以上動いていなければ、
//	今回は新規サンプルを追加しない(フレームレート依存のサンプル密集/間延びを防ぐ)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::UpdateTipBase(const DirectX::SimpleMath::Vector3& tip, const DirectX::SimpleMath::Vector3& base)
{
	if (!isRecording_) { return; }

	const float minDistSq = params_.MinSampleDistance * params_.MinSampleDistance;
	const bool shouldRecord = !hasLastRecordedTip_ ||
		(tip - lastRecordedTip_).LengthSquared() >= minDistSq;

	if (!shouldRecord) { return; }

	SlashTrailSample sample;
	sample.Tip = tip;
	sample.Base = base;
	sample.Age = 0.0f;
	samples_.push_back(sample);

	lastRecordedTip_ = tip;
	hasLastRecordedTip_ = true;

	// MinSampleDistanceによる間引きだけでは対応できない(想定外に速い/長時間の記録)ケースの安全弁
	TrimOverflowSamples();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 記録終了：新規サンプルの記録を止めるだけ(既存サンプルはUpdate()のたびに自然にフェードする)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::EndRecording()
{
	isRecording_ = false;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 毎フレーム更新：Age加算→期限切れサンプルの間引き→Draw用頂点配列の再構築
//	※新規サンプルが増えたかどうかに関わらず、Ageが進んだ分だけ色・幅が変化するため、
//	  頂点の再構築は毎フレーム必要
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::Update(float deltaTime)
{
	for (auto& sample : samples_)
	{
		sample.Age += deltaTime;
	}

	TrimExpiredSamples();
	RebuildVertices();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画：DrawPassFlagsにpassが含まれ、かつ描画できるだけの頂点が揃っている時だけ描画する
//	※GPU側(動的頂点バッファへのMap/Unmap、シェーダーのセット、DrawCall)は未実装。
//	  実装する際は、この時点のvertices_(GetVertices())をそのままコピーすれば良い
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::Draw(ParticleDrawPass pass) const
{
	if (!KdHasDrawPassFlag(params_.DrawPassFlags, pass)) { return; }

	// 三角形ストリップを組むには最低2サンプル(4頂点)必要
	if (vertices_.size() < 4) { return; }

	// TODO: GPU側実装
	//  ・動的頂点バッファ(D3D11_USAGE_DYNAMIC + D3D11_CPU_ACCESS_WRITE)をMap(WRITE_DISCARD)し、
	//    vertices_(GetVertices())をコピーしてUnmap
	//  ・専用の頂点シェーダー(Position/UV/ColorのInputLayoutを受け取り、
	//    ワールド×ビュー×プロジェクション変換するだけの単純なもの)をセット
	//  ・ピクセルシェーダーはKdGPUParticle_PS.hlslの流用を検討(中身次第)
	//  ・shaderMgr.ChangeBlendState()をparams_.BlendModeに応じて切り替え(KdGPUParticle::Drawと同じ要領)
	//  ・DevCon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP)
	//  ・DevCon->Draw((UINT)vertices_.size(), 0)
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 先頭(最古)から見て、Age > FadeLengthになったサンプルを間引く
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::TrimExpiredSamples()
{
	while (!samples_.empty() && samples_.front().Age > params_.FadeLength)
	{
		samples_.pop_front();
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// MaxSamplesを超えた分を、先頭(最古)から強制的に間引く(安全弁)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::TrimOverflowSamples()
{
	while (samples_.size() > params_.MaxSamples)
	{
		samples_.pop_front();
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// samples_から、Draw用の頂点配列(vertices_)を作り直す
//	各サンプルをTip/Baseの2頂点に変換し、三角形ストリップ用に交互に並べる
//	(頂点順：s0.Tip, s0.Base, s1.Tip, s1.Base, ... となるようpush_backしていく)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::RebuildVertices()
{
	vertices_.clear();

	// 三角形ストリップを組むには最低2サンプル必要(1サンプルでは面にならない)
	if (samples_.size() < 2) { return; }

	vertices_.reserve(samples_.size() * 2);

	using namespace DirectX::SimpleMath;

	// FadeLengthが0以下(設定ミス)の場合に0除算にならないようにする
	const float fadeLengthSafe = (params_.FadeLength > 0.0f) ? params_.FadeLength : 0.0001f;

	for (const auto& sample : samples_)
	{
		// 1(記録直後)～0(消える直前)。UV.x・アルファ・幅のテーパー全てこれを元に計算する
		const float fadeRate = std::clamp(1.0f - sample.Age / fadeLengthSafe, 0.0f, 1.0f);

		// 古いサンプルほど、Tip-Base間の中心へ向けて幅を萎ませる。
		// fadeRate=1(新しい)ならtaper=1(満幅)、fadeRate=0(消える直前)ならtaper=1-TipWidthTaper
		// (TipWidthTaper=1なら中心の1点まで萎み、0なら常に満幅のまま変化しない)
		const float taper = 1.0f - (1.0f - fadeRate) * params_.TipWidthTaper;

		const Vector3 center = (sample.Tip + sample.Base) * 0.5f;
		const Vector3 tipAdj = Vector3::Lerp(center, sample.Tip, taper);
		const Vector3 baseAdj = Vector3::Lerp(center, sample.Base, taper);

		SlashTrailVertex vTip;
		vTip.Position = tipAdj;
		vTip.UV = { fadeRate, 0.0f };
		vTip.Color = { 1.0f, 1.0f, 1.0f, fadeRate };
		vertices_.push_back(vTip);

		SlashTrailVertex vBase;
		vBase.Position = baseAdj;
		vBase.UV = { fadeRate, 1.0f };
		vBase.Color = { 1.0f, 1.0f, 1.0f, fadeRate };
		vertices_.push_back(vBase);
	}
}
