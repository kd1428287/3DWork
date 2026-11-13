#include "../GPUParticle/inc_GPUParticleNoise.hlsli"	// ※実際のパスに合わせて要調整(b4/t2/s1を使う)

// ShapeEffectShader::cbShape と一致させる事(VSと同じ並び)
cbuffer cbShapeEffect : register(b5)
{
	row_major float4x4	g_mWorld;
	float4				g_Color;
	float				g_DepthOffset;
	float				g_Alpha;	// 動きのalpha。ディゾルブ有効時は消去の進行度(1=見える、0=消える)
	float				g_Time;		// 経過秒
	float				g_pad;
};

Texture2D		g_tex : register(t0);
SamplerState	g_ss  : register(s0);

struct VSOutput
{
	float4 Pos : SV_Position;
	float2 UV  : TEXCOORD0;
};

float4 main(VSOutput In) : SV_Target0
{
	float4 outColor = g_tex.Sample(g_ss, In.UV) * g_Color;

	// ディゾルブ有効時は線形フェードの代わりに、alphaを進行度としてノイズで消す(パーティクルと同じ)
	if (!(g_NoiseFlags & NOISE_DISSOLVE)) { outColor.a *= g_Alpha; }

	// 共通のノイズ関数はスクロールを「寿命の進行(1-lifeRate)」で動かすため、seedで打ち消して時間(秒)駆動にする
	const float2 seed = g_NoiseScroll * (g_Time - (1.0f - g_Alpha));

	return ApplyParticleNoise(outColor, In.UV, g_Alpha, seed);
}
