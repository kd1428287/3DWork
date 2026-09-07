#include "../main.h"
#include "SlashTrailInstance.h"

#include <algorithm>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 初期化：パラメータを保持し、GPU描画用リソース(KdSlashTrailRenderer)を生成する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool SlashTrailInstance::Init(const SlashTrailParams& params)
{
	params_ = params;

	samples_.clear();
	vertices_.clear();

	isRecording_ = false;
	hasLastRecordedTip_ = false;

	// 頂点バッファはMaxSamples * 2(三角形ストリップ用に2頂点/サンプル)ぶん確保しておく
	renderer_ = std::make_shared<SlashTrailRenderer>();
	if (!renderer_->Init(params_.MaxSamples * 2))
	{
		renderer_.reset();
		return false;
	}

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
// 描画：DrawPassFlagsにpassが含まれる時だけ、KdSlashTrailRendererへ委譲して描画する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailInstance::Draw(ParticleDrawPass pass) const
{
	if (!KdHasDrawPassFlag(params_.DrawPassFlags, pass)) { return; }
	if (!renderer_) { return; }

	renderer_->Draw(vertices_, params_.BlendMode);
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
//
//	【カメラ正対補正】
//	Tip-Base(剣の実座標)をそのまま幅として使うと、カメラから見て剣がエッジオン
//	(視線方向と平行)に近づくほど画面上の幅がほぼ0になり、帯が消えたように見えてしまう。
//	これを防ぐため、カメラ視線に対して垂直な「カメラ正対ベクトル(camRight)」を用意し、
//	エッジオンの度合い(alignment)が高いほど、実座標由来の幅ベクトルからそちらへ
//	ブレンドして最低限の見た目の幅を確保する。
//	alignment==0(通常通り横から見えている状態)では元の計算と完全に一致する
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

	// カメラ座標を取得(エッジオン補正用。頂点構築自体がCPU側で行われる設計の為、
	// ここで直接カメラ定数バッファを参照する。GPU(VS)側へ元データを渡して計算させる方式は
	// 頂点レイアウト・VSInputの変更が必要になる為採用しない)
	const Vector3 camPos = KdShaderManager::Instance().GetCameraCB().CamPos;

	for (const auto& sample : samples_)
	{
		// 1(記録直後)～0(消える直前)。UV.x・アルファ・幅のテーパー全てこれを元に計算する
		const float fadeRate = std::clamp(1.0f - sample.Age / fadeLengthSafe, 0.0f, 1.0f);

		// 古いサンプルほど、Tip-Base間の中心へ向けて幅を萎ませる。
		// fadeRate=1(新しい)ならtaper=1(満幅)、fadeRate=0(消える直前)ならtaper=1-TipWidthTaper
		// (TipWidthTaper=1なら中心の1点まで萎み、0なら常に満幅のまま変化しない)
		const float taper = 1.0f - (1.0f - fadeRate) * params_.TipWidthTaper;

		const Vector3 center = (sample.Tip + sample.Base) * 0.5f;

		// ----- カメラ正対補正 -----
		Vector3 widthVec = sample.Tip - sample.Base;
		const float widthLen = widthVec.Length();

		Vector3 correctedWidthVec = widthVec;
		if (widthLen > 1e-5f)
		{
			Vector3 viewDir = camPos - center;
			const float viewDirLenSq = viewDir.LengthSquared();
			if (viewDirLenSq > 1e-6f)
			{
				viewDir /= sqrtf(viewDirLenSq);

				// カメラ正対ベクトル：視線方向とワールドUpに直交する「カメラの右方向」
				Vector3 camRight = viewDir.Cross(Vector3::Up);
				if (camRight.LengthSquared() < 1e-6f)
				{
					// カメラがほぼ真上/真下を向いている場合の保険
					camRight = viewDir.Cross(Vector3::Right);
				}
				camRight.Normalize();

				// widthVecの向きにcamRightの符号を合わせる(逆だと帯がねじれて見える為)
				if (widthVec.Dot(camRight) < 0.0f) { camRight = -camRight; }

				// widthVecのうち視線方向に平行な成分の割合(0=真横に見える～1=真正面でエッジオン)
				const float alignment = fabsf(widthVec.Dot(viewDir)) / widthLen;

				// エッジオンに近いほど、実座標由来の幅ベクトルから
				// カメラ正対ベクトル(元の長さを維持)へブレンドする
				correctedWidthVec = Vector3::Lerp(widthVec, camRight * widthLen, alignment);
			}
		}

		const Vector3 halfWidth = correctedWidthVec * 0.5f;
		const Vector3 tipAdj = Vector3::Lerp(center, center + halfWidth, taper);
		const Vector3 baseAdj = Vector3::Lerp(center, center - halfWidth, taper);

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