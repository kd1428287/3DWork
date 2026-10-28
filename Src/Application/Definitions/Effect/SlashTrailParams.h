#pragma once

#include "EffectParams.h"	// ParticleBlendMode / ParticleDrawPass / KdHasDrawPassFlag を流用する為
#include "Framework/Shader/SlashTrailShader/SlashTrailRenderer.h"	// SlashTrailVertex(=Vertex)の実体を流用する為

// トレイル(斬撃の軌跡)の描画用1頂点
using SlashTrailVertex = SlashTrailRenderer::Vertex;

// 記録された1サンプル(剣のTip/Base座標のペア)
struct SlashTrailSample
{
	Math::Vector3	Tip;
	Math::Vector3	Base;

	// 始点(最初のサンプル)からのTip軌跡の累積距離
	float							Dist = 0.0f;

	// 平滑化済みの移動速度(単位/秒)
	float							Speed = 0.0f;
};

// トレイル1本ぶんの発生パラメータ
struct SlashTrailParams
{
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// ストローク(一筆)
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

	// 想定ストローク長(UVの基準長)。始点からこの距離でu=1に達し、超えた分はu=1に固定される
	float	StrokeLength = 8.0f;

	// 直前のサンプルからこの距離以上動いたら新規サンプルを記録する
	float	MinSampleDistance = 0.02f;

	// サンプル数の上限。到達したらサンプル追加を止める
	UINT	MaxSamples = 192;

	// 先頭からこの距離のあいだ、幅を0→1へ絞る(0以下なら絞らない)
	float	HeadTaperLength = 0.6f;

	// 記録終了後、全体が一括で消えるまでの時間(秒)
	float	DissolveTime = 0.35f;

	// 記録開始からこの秒数でEndRecording()を自動で呼ぶ(0以下なら無期限。Endが来ない場合の保険)
	float	MaxRecordingTime = 10.0f;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// 速度ベースの太さ変調(筆の力強さの演出)
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	float	SpeedWidthReference = 14.0f;
	float	MinSpeedWidthScale = 0.35f;
	float	MaxSpeedWidthScale = 2.6f;

	// 速度平滑化の時定数(秒)。フレームレートに依存しない
	float	SpeedSmoothingTime = 0.05f;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// 軌跡のスプライン平滑化(Catmull-Rom)
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	bool	SplineEnabled = true;
	UINT	SplineSubdivisions = 4;
	UINT	SplineMaxPoints = 384;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// 色(始点側のColdColor → 先頭側のHotColorへ、u=0→1で補間)
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	DirectX::SimpleMath::Vector3	HotColor = { 0.01f, 0.01f, 0.01f };
	DirectX::SimpleMath::Vector3	ColdColor = { 0.001f, 0.001f, 0.001f };

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// 芯(コア)レイヤー:墨の帯とは別に、細く明るい加算合成の芯を重ねる
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	bool	CoreEnabled = true;
	float	CoreWidthScale = 0.1f;
	DirectX::SimpleMath::Vector3	CoreColor = { 0.45f, 0.325f, 0.3f };

	// 芯の始点側のα(先頭側は1。0なら始点へ向かって薄れる)
	float	CoreTailAlpha = 0.0f;
	ParticleBlendMode	CoreBlendMode = ParticleBlendMode::Add;
	ParticleDrawPass	CoreDrawPassFlags = ParticleDrawPass::Default;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// マテリアル関連
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// Asset/Textures/Game/Effect/からの相対パス(パーティクルのTexturePathと同じ規約)
	std::string				TexturePath = "Trail2.png";
	ParticleBlendMode		BlendMode = ParticleBlendMode::Alpha;
	ParticleDrawPass		DrawPassFlags = ParticleDrawPass::Default;
};
