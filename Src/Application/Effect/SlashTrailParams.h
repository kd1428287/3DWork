#pragma once

#include "EffectParams.h"	// KdParticleBlendMode / KdParticleDrawPass / KdHasDrawPassFlag を流用する為
#include "../../Framework/Shader/SlashTrailShader/SlashTrailRenderer.h"	// SlashTrailVertex(=Vertex)の実体を流用する為
#include <string>

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
	float							Speed = 0.0f;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル1本ぶんの発生パラメータ(GPUParticleParamsに相当する定義データ)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct SlashTrailParams
{
	// 記録されてからこの秒数でフェードアウトし切る(サンプル1個分の生存時間)
	float	FadeLength = 0.5f;

	// 直前に記録したTip座標からこの距離以上動いたら新規サンプルを記録する
	float	MinSampleDistance = 0.005f;

	// 0～1。古いサンプルほど帯の幅をTip-Base間の中心へ萎ませる強さ
	float	TipWidthTaper = 1.0f;

	// サンプル数のハード上限
	UINT	MaxSamples = 128;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// 速度ベースの太さ変調(筆の力強さの演出)
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	float	SpeedWidthReference = 8.0f;
	float	MinSpeedWidthScale = 0.35f;
	float	MaxSpeedWidthScale = 1.6f;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// 時間経過による滲み(墨が紙に染み込んで広がる表現)
	//	・記録直後(Age=0)はBleedStartScaleの細さから始まり、
	//	  BleedPeakTime秒後にBleedPeakScaleまで太く滲む
	//	・そこから先はFadeLengthに向けて1.0(通常幅)へ滑らかに戻り、
	//	  最終的な収束(消え際の萎み)はTipWidthTaperにそのまま委ねる
	//	・speedScale(力強さ)・taper(消え際の萎み)とは独立して幅に乗算される
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

	// 滲み処理を行うか
	bool	BleedEnabled = true;

	// 記録されてからこの秒数でBleedPeakScaleに達する(FadeLengthより十分小さい値を想定)
	float	BleedPeakTime = 0.5f; //0.05f;

	// 記録直後(Age=0)の太さ倍率。1.0未満で「最初は細い」演出になる
	float	BleedStartScale = 0.75f;

	// 滲みのピーク時の太さ倍率
	float	BleedPeakScale = 3.0f;// 1.35f;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// 色レイヤー分離用(fadeRateによる2色補間。1=新しい側/剣の現在位置、0=古い側/消える直前)
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

	DirectX::SimpleMath::Vector3	HotColor = { 0.05f, 0.05f, 0.05f };
	DirectX::SimpleMath::Vector3	ColdColor = { 0.001f, 0.001f, 0.001f };

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// 芯(コア)レイヤー:墨の帯とは別に、細く明るい加算合成の芯を重ねて視認性を確保する
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

	// 芯レイヤーを描画するか
	bool	CoreEnabled = true;

	// 芯の太さ。墨の帯(taper・速度変調適用後)の幅に対する比率(0～1)
	float	CoreWidthScale = 0.03f;

	// 芯の色。白〜淡い黄色を想定。Add合成前提のため1.0を超える強めの値も可
	DirectX::SimpleMath::Vector3	CoreColor = { 0.15f, 0.13f, 0.10f }; //{ 0.55f, 0.45f, 0.35f };

	// 芯の合成方法。加算合成(Add)を想定
	KdParticleBlendMode	CoreBlendMode = KdParticleBlendMode::Add;

	// 芯だけを描画するパス。墨レイヤー(DrawPassFlags)と別に指定できる
	ParticleDrawPass	CoreDrawPassFlags = ParticleDrawPass::Default | ParticleDrawPass::Blight;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// マテリアル関連
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	std::string				TexturePath = "Asset/Textures/Game/Effect/Trail4.png";
	KdParticleBlendMode		BlendMode = KdParticleBlendMode::Alpha;
	ParticleDrawPass		DrawPassFlags = ParticleDrawPass::Default;
};