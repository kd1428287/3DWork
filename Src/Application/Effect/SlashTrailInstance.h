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

	// コピー禁止(GPU描画用リソース(KdSlashTrailRenderer)をshared_ptrで持つ為。
	// EffectInstanceがKdGPUParticleをshared_ptrで持つのと同じ理由)
	SlashTrailInstance(const SlashTrailInstance&) = delete;
	SlashTrailInstance& operator=(const SlashTrailInstance&) = delete;

	// ムーブは許可(shared_ptrなので安全にムーブできる。生ポインタを直接持たせていない為、
	// デフォルトのムーブでも二重解放は起きない)
	SlashTrailInstance(SlashTrailInstance&&) = default;
	SlashTrailInstance& operator=(SlashTrailInstance&&) = default;

	// パラメータを保持し、GPU描画用リソース(KdSlashTrailRenderer)を生成する。
	// 頂点バッファの容量はparams.MaxSamples * 2(三角形ストリップ用)で確保される
	bool Init(const SlashTrailParams& params);

	// 記録開始(攻撃開始時に呼ぶ)。既存サンプルは破棄してゼロから記録し直す
	void BeginRecording();

	// 記録中、毎フレーム剣のTip/Base座標を渡す。
	//	記録中(isRecording_==true)でない場合は無視される。
	//	前回記録したTipからMinSampleDistance以上動いていない場合は、
	//	今回は新規サンプルを追加しない(間引き)
	void UpdateTipBase(const DirectX::SimpleMath::Vector3& tip, const DirectX::SimpleMath::Vector3& base);

	// 記録終了(攻撃終了時に呼ぶ)。isRecording_をfalseにするだけで、
	//	既存サンプルには触らない(＝新規サンプルが増えなくなるだけで、
	//	既存分はUpdate()のたびにAgeが進み、自然にフェードアウトしていく)
	void EndRecording();

	// 毎フレーム呼ぶ：Age更新→期限切れサンプルの間引き→Draw用頂点配列の再構築、の順で行う
	//	※新規サンプルが増えたかどうかに関わらず、Ageが進んだ分だけ色・幅が変化する為、
	//	  頂点の再構築は毎フレーム必要になる点に注意
	void Update(float deltaTime);

	// paramsのDrawPassFlagsにpassが含まれる時だけ描画する(EffectInstance::Draw(pass)と同じ規約)
	void Draw(ParticleDrawPass pass) const;

	bool IsRecording() const { return isRecording_; }

	// 記録が終わっており(EndRecording済み)、かつ既存サンプルも全てフェードアウトし切った状態か。
	//	呼び出し側(ディスパッチャー相当)はこれがtrueになったタイミングで初めてインスタンスを破棄する事
	bool IsFinished() const { return !isRecording_ && samples_.empty(); }

	const SlashTrailParams& GetParams() const { return params_; }

	// Draw用に構築済みのCPU側頂点配列(GPU側実装時、ここをそのまま動的頂点バッファへ
	// Map/Unmapでコピーする想定。三角形ストリップ用に2頂点×サンプル数ぶん並んでいる)
	const std::vector<SlashTrailVertex>& GetVertices() const { return vertices_; }

private:

	// 先頭(最古)から見て、Age > FadeLengthになったサンプルを間引く
	void TrimExpiredSamples();

	// MaxSamplesを超えた分を、先頭(最古)から強制的に間引く(安全弁)
	void TrimOverflowSamples();

	// samples_から、Draw用の頂点配列(vertices_)を作り直す
	void RebuildVertices();

	SlashTrailParams	params_;

	std::deque<SlashTrailSample>	samples_;	// 先頭=最古、末尾=最新

	bool	isRecording_ = false;

	// UpdateTipBase()での距離判定用。まだ1件も記録していない間はhasLastRecordedTip_==false
	bool							hasLastRecordedTip_ = false;
	DirectX::SimpleMath::Vector3	lastRecordedTip_ = { 0.0f, 0.0f, 0.0f };

	// Update()のたびに再構築される、Draw用のCPU側頂点配列
	std::vector<SlashTrailVertex>	vertices_;

	// GPU描画用リソース(頂点バッファ・専用VS・KdGPUParticle_PS流用のPS)。
	// shared_ptrで持つ理由はコピー禁止/ムーブ許可の説明を参照
	std::shared_ptr<SlashTrailRenderer>	renderer_;
};