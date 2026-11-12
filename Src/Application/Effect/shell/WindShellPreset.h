#pragma once

#include <string>

#include "Framework/Shader/GPUParticle/ParticleBuffer.h"	// ParticleNoiseParams

// 風シェル1種類ぶんの見た目設定。0を付けた値はProgress=0、1を付けた値はProgress=1のときの値
struct WindShellPreset
{
	std::string	StreakTexturePath = "Asset/Textures/Game/Effect/WindStreakInk.png";	// 筋(縦に伸ばして使う)
	std::string	NoiseTexturePath = "Asset/Textures/Game/Effect/WindNoiseInk.png";	// 消去ディゾルブ用

	float	Length0 = 1.8f,		Length1 = 1.8f;		// 円筒部の長さ(0で球)
	float	Radius0 = 1.0f,	Radius1 = 01.0f;
	float	Twist0 = 8.0f,		Twist1 = 14.0f;
	float	SpinSpeed0 = 5.0f,	SpinSpeed1 = 5.0f;	// ラジアン/秒

	float	Taper = 0.6f;			// 後方のすぼまり(0-1)
	float	NoiseAmp = 0.5f;		// 頂点ノイズによる半径の揺らぎ
	float	NoiseFreq = 1.5f;
	float	StreakCount = 4.0f;		// 周方向の筋の繰り返し数(整数)
	float	StreakStretch = 0.1f;	// 軸方向のUV倍率(小さいほど長い筋)
	float	FlowSpeed = 3.4f;
	float	Threshold = 0.15f;
	float	Softness = 0.03f;
	float	RimPower = 0.0f;
	float	Scatter = 0.6f;		// 筋UVの歪み量(0で規則的)
	float	SpinJitter = 0.6f;	// 位置ごとの回転角のゆらぎ(ラジアン)
	float	NoiseSpeed = 0.5f;	// 半径ノイズの時間変化の速さ

	// 墨(Alpha)と芯(Add)の2層
	Math::Vector4	InkColor = { 0.05f, 0.05f, 0.06f, 1.0f };
	float			InkAlpha = 0.6f;
	Math::Vector4	CoreColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	float			CoreAlpha = 0.5f;
	float			CoreRadiusScale = 0.8f;
	float			CoreLengthScale = 0.9f;
	float			CoreSpinScale = -1.0f;	// 墨に対する回転速度の倍率(負で逆回転)
	float			CoreSeedOffset = 3.0f;

	float	FadeInTime = 0.15f;
	float	FadeOutTime = 0.4f;

	ParticleNoiseParams	Noise = MakeDefaultNoise();

	// 消去用ノイズ(ディゾルブのみ。flagsは HLSL の NOISE_DISSOLVE = 1<<1)
	static ParticleNoiseParams MakeDefaultNoise()
	{
		ParticleNoiseParams n;
		n.flags = (1u << 1);
		n.noiseScale = 1.0f;	// 継ぎ目が出ないよう整数にする
		n.scroll = { 0.0f, 0.0f };
		n.maskPower = 1.0f;
		n.dissolveStart = 0.0f;
		n.dissolveSoftness = 0.2f;
		n.edgeWidth = 0.0f;
		n.edgeColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		return n;
	}

	// 敵が身にまとう風(カプセル)
	static WindShellPreset EnemyAura()
	{
		return WindShellPreset();
	}

	// チャージ演出(球。Progressで半径と回転が増す)
	static WindShellPreset Charge()
	{
		WindShellPreset p;
		p.Length0 = p.Length1 = 0.0f;
		p.Radius0 = 0.3f;	p.Radius1 = 1.0f;
		p.Twist0 = 6.0f;	p.Twist1 = 10.0f;
		p.SpinSpeed0 = 3.0f;	p.SpinSpeed1 = 10.0f;
		p.Taper = 0.0f;
		p.NoiseAmp = 0.2f;
		return p;
	}
};
