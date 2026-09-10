#pragma once

#include "EffectParams.h"	// KdParticleBlendMode / KdParticleDrawPass / KdHasDrawPassFlag を流用する為
#include "../../Framework/Shader/SlashTrailShader/SlashTrailRenderer.h"	// SlashTrailVertex(=Vertex)の実体を流用する為
#include <string>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル(斬撃の軌跡)の描画用1頂点
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 実体はKdSlashTrailRenderer::Vertex(低レイヤー側)で定義されている。
// KdGPUParticle::ParticleがKdGPUParticle.h側で定義されているのと同じ考え方で、
// GPU資源を扱うクラスに実際の頂点レイアウトの定義を置き、Effect側はそれを
// そのまま使う(エイリアスを貼るだけ)にしてある
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
using SlashTrailVertex = SlashTrailRenderer::Vertex;

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 記録された1サンプル(剣のTip/Base座標のペア)
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// Ageは記録されてからの経過秒数。SlashTrailInstance::Update()で毎フレーム加算され、
// SlashTrailParams::FadeLengthを超えたサンプルから間引かれる
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct SlashTrailSample
{
	DirectX::SimpleMath::Vector3	Tip;
	DirectX::SimpleMath::Vector3	Base;
	float							Age = 0.0f;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル1本ぶんの発生パラメータ(GPUParticleParamsに相当する定義データ)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct SlashTrailParams
{
	// 記録されてからこの秒数でフェードアウトし切る(サンプル1個分の生存時間)
	float	FadeLength = 0.25f;

	// 直前に記録したTip座標からこの距離以上動いたら新規サンプルを記録する
	//	(毎フレーム無条件に記録すると、フレームレート次第でサンプルが密集/間延びする為の間引き。
	//	 時間ベースではなく距離ベースにすることで、振りの速さに自動で追従する)
	float	MinSampleDistance = 0.005f;

	// 0～1。古いサンプルほど帯の幅をTip-Base間の中心へ萎ませる強さ
	//	0で萎ませない(常に満幅)、1で消える直前に幅0(中心の1点)まで萎む
	float	TipWidthTaper = 1.0f;

	// サンプル数のハード上限(GPU側の頂点バッファサイズの見積もりにも使う安全弁)。
	//	MinSampleDistanceによる間引きだけでは、剣が速すぎてFadeLength以内に
	//	大量のサンプルが溜まるケースに対応できない為
	UINT	MaxSamples = 64;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// マテリアル関連(GPU側実装時に使用。現時点ではデータとして保持するのみ)
	//	GPUパーティクルと同じ語彙(KdParticleBlendMode/KdParticleDrawPass)を
	//	そのまま使い回し、質感の管理方法を統一している
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	std::string				TexturePath;
	KdParticleBlendMode		BlendMode = KdParticleBlendMode::Add;
	ParticleDrawPass		DrawPassFlags = ParticleDrawPass::Default;// | ParticleDrawPass::Blight;
};