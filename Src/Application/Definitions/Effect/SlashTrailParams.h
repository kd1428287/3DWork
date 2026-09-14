#pragma once

#include "EffectParams.h"	// KdParticleBlendMode / KdParticleDrawPass / KdHasDrawPassFlag を流用する為
#include "Framework/Shader/SlashTrailShader/SlashTrailRenderer.h"	// SlashTrailVertex(=Vertex)の実体を流用する為

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル(斬撃の軌跡)の描画用1頂点
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
using SlashTrailVertex = SlashTrailRenderer::Vertex;

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 記録された1サンプル(剣のTip/Base座標のペア)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct SlashTrailSample
{
	DirectX::SimpleMath::Vector3	Tip;
	DirectX::SimpleMath::Vector3	Base;
	float							Age = 0.0f;

	// 平滑化済みの移動速度(単位/秒)
	float							Speed = 0.0f;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル1本ぶんの発生パラメータ(GPUParticleParamsに相当する定義データ)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct SlashTrailParams
{
	// 記録されてからこの秒数でフェードアウトし切る(サンプル1個分の生存時間)
	float	FadeLength = 0.75f;

	// 直前に記録したTip座標からこの距離以上動いたら新規サンプルを記録する
	float	MinSampleDistance = 0.005f;

	// 0～1。古いサンプルほど帯の幅をTip-Base間の中心へ萎ませる強さ
	float	TipWidthTaper = 1.0f;

	// サンプル数のハード上限
	UINT	MaxSamples = 64;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// 速度ベースの太さ変調(筆の力強さの演出)
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	float	SpeedWidthReference = 8.0f;
	float	MinSpeedWidthScale = 0.35f;
	float	MaxSpeedWidthScale = 1.6f;
	float	SpeedSmoothingAlpha = 0.25f;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// 時間経過による滲み(墨が紙に染み込んで広がる表現)
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	bool	BleedEnabled = true;
	float	BleedPeakTime = 0.05f;
	float	BleedStartScale = 0.75f;
	float	BleedPeakScale = 1.35f;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// UVマッピング方式(テクスチャの非等幅な形状が伸び縮みして見える「電動ノコギリ」対策)
	//	・trueの場合:UV.xを「現在の切っ先からの累積距離 ÷ UVTileLength」で求め、
	//	  mod 1.0でタイリングする
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	bool	UseDistanceBasedUV = true;
	float	UVTileLength = 1.2f;

	// 距離ベースUVにおける「直近に記録されたサンプルから、現在の実際の剣先までの延長距離
	// (tipOffset)」の平滑化係数(0より大きく1以下)。
	//	記録中はUpdateTipBase()が毎フレーム呼ばれ、剣先が動いた分だけこの延長距離が
	//	連続的に伸びる。振りが速いフレームほど1フレームあたりの伸び量が大きくなり、
	//	墨のテクスチャが等幅でない(かすれ・にじみの模様を持つ)場合、UVが大きく
	//	進むたびにサンプリングされる模様が大きく切り替わり、帯全体の太さが
	//	一斉に脈打つように見える(速度が速いほど悪化する)。
	//	この値でEMA(指数移動平均)を掛けて延長距離の変化を滑らかにすることで、
	//	急激なUV進行を抑える。1.0に近いほど生の値に近く(脈動が残りやすい)、
	//	0に近いほど滑らかだが、非常に速い振りでは実際の剣先位置に対して
	//	テクスチャの模様がわずかに遅れて追従する
	float	TipOffsetSmoothingAlpha = 0.15f;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// 軌跡のスプライン平滑化(Catmull-Rom)
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	bool	SplineEnabled = true;
	UINT	SplineSubdivisions = 4;
	UINT	SplineMaxPoints = 192;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// 色レイヤー分離用(fadeRateによる2色補間。1=新しい側/剣の現在位置、0=古い側/消える直前)
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	DirectX::SimpleMath::Vector3	HotColor = { 0.05f, 0.05f, 0.05f };
	DirectX::SimpleMath::Vector3	ColdColor = { 0.001f, 0.001f, 0.001f };

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// 芯(コア)レイヤー:墨の帯とは別に、細く明るい加算合成の芯を重ねて視認性を確保する
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	bool	CoreEnabled = true;
	float	CoreWidthScale = 0.1f;
	DirectX::SimpleMath::Vector3	CoreColor = { 0.45f, 0.325f, 0.3f };
	KdParticleBlendMode	CoreBlendMode = KdParticleBlendMode::Add;
	ParticleDrawPass	CoreDrawPassFlags = ParticleDrawPass::Default;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// マテリアル関連
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	std::string				TexturePath;
	KdParticleBlendMode		BlendMode = KdParticleBlendMode::Alpha;
	ParticleDrawPass		DrawPassFlags = ParticleDrawPass::Default;// | ParticleDrawPass::Bright;
};