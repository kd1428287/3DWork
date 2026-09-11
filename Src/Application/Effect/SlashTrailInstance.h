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
// GPUパーティクルの「テンプレート/使い回し1体」という発想とは違い、記録開始のたびに
// 呼び出し側(ディスパッチャー相当)がInstanceKeyごとに新規生成して使う想定。
//
// 【対象範囲】
//	CPU側のロジック(状態遷移・サンプル記録・間引き・頂点データ構築)に加え、
//	GPU側の描画(KdSlashTrailRenderer経由の動的頂点バッファ描画)まで含む。
//	ピクセルシェーダーはKdGPUParticle_PS.hlslをそのまま流用しており、
//	頂点シェーダー(KdSlashTrail_VS.hlsl)のみ新規に用意した。
//
// 【墨レイヤー / 芯(コア)レイヤーについて】
//	同じsamples_履歴から、墨の帯(vertices_)と、細く明るい加算合成の芯(coreVertices_)の
//	2種類の頂点配列を毎フレーム構築する。GPUリソースはrenderer_(動的頂点バッファ1個)を
//	使い回し、Draw()内で「墨レイヤー描画→芯レイヤー描画」の順にDraw呼び出しを2回行う
//	(Map(WRITE_DISCARD)→memcpy→Unmap→Drawが呼び出しごとに完結するため、
//	 同一バッファを連続で使い回しても問題ない)
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
//	※GPUパーティクルのEffectDetachEvent(即座に破棄)とは違い、EndRecording()は
//	  フラグを倒すだけで、既存サンプルには触らない。呼び出し側は毎フレーム
//	  IsFinished()を確認し、trueになって初めて破棄すること(でないとフェードアウトの
//	  途中で帯が一瞬で消えてしまう)
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

	// パラメータを保持し、GPU描画用リソース(KdSlashTrailRenderer)を生成する。
	// 頂点バッファの容量はparams.MaxSamples * 2(三角形ストリップ用)で確保される。
	// 墨レイヤー・芯レイヤーは同じバッファを使い回すため、容量はレイヤー間で共用でよい
	bool Init(const SlashTrailParams& params);

	// 記録開始(攻撃開始時に呼ぶ)。既存サンプルは破棄してゼロから記録し直す
	void BeginRecording();

	// 記録中、毎フレーム剣のTip/Base座標を渡す。
	//	記録中(isRecording_==true)でない場合は無視される。
	//	前回記録したTipからMinSampleDistance以上動いていない場合は、
	//	今回は新規サンプルを追加しない(間引き)。
	//	新規サンプル記録時、直前サンプルからの経過時間(sinceLastSampleTime_)と
	//	移動距離からSlashTrailSample::Speedを算出する(速度ベースの太さ変調に使用)
	void UpdateTipBase(const DirectX::SimpleMath::Vector3& tip, const DirectX::SimpleMath::Vector3& base);

	// 記録終了(攻撃終了時に呼ぶ)。isRecording_をfalseにするだけで、
	//	既存サンプルには触らない
	void EndRecording();

	// 毎フレーム呼ぶ：sinceLastSampleTime_加算→Age更新→期限切れサンプルの間引き→
	//	Draw用頂点配列(墨レイヤー・芯レイヤー両方)の再構築、の順で行う
	void Update(float deltaTime);

	// paramsのDrawPassFlags/CoreDrawPassFlagsにpassが含まれる時だけ、
	//	それぞれのレイヤーを描画する(EffectInstance::Draw(pass)と同じ規約)
	void Draw(ParticleDrawPass pass) const;

	bool IsRecording() const { return isRecording_; }

	bool IsFinished() const { return !isRecording_ && samples_.empty(); }

	const SlashTrailParams& GetParams() const { return params_; }

	// Draw用に構築済みのCPU側頂点配列(墨レイヤー)
	const std::vector<SlashTrailVertex>& GetVertices() const { return vertices_; }

	// Draw用に構築済みのCPU側頂点配列(芯レイヤー)
	const std::vector<SlashTrailVertex>& GetCoreVertices() const { return coreVertices_; }

private:

	void TrimExpiredSamples();

	void TrimOverflowSamples();

	// samples_から、Draw用の頂点配列(墨レイヤー:vertices_、芯レイヤー:coreVertices_)を作り直す
	void RebuildVertices();

	SlashTrailParams	params_;

	std::deque<SlashTrailSample>	samples_;	// 先頭=最古、末尾=最新

	bool	isRecording_ = false;

	bool							hasLastRecordedTip_ = false;
	DirectX::SimpleMath::Vector3	lastRecordedTip_ = { 0.0f, 0.0f, 0.0f };

	// 直前にサンプルを記録してからの経過時間(秒)。速度算出に使用
	float	sinceLastSampleTime_ = 0.0f;

	// Update()のたびに再構築される、Draw用のCPU側頂点配列(墨の帯)
	std::vector<SlashTrailVertex>	vertices_;

	// Update()のたびに再構築される、Draw用のCPU側頂点配列(白熱した芯。加算合成)
	std::vector<SlashTrailVertex>	coreVertices_;

	// GPU描画用リソース(頂点バッファ・専用VS・KdGPUParticle_PS流用のPS)。
	// 墨レイヤー・芯レイヤーの両方でこの1個を使い回す(Draw()を2回呼ぶだけで済むため)
	std::shared_ptr<SlashTrailRenderer>	renderer_;
};