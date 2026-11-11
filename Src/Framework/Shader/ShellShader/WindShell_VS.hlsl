#include "../inc_KdCommon.hlsli"
#include "../PerlinNoise.hlsli"		// 順列テーブルはVSのt10(PERLIN_PERM_SLOT)
#include "inc_WindShell.hlsli"

struct VSInput
{
	float2 UV : TEXCOORD0;	// x=角度(0-1) y=軸方向(0-1。0が後方)
};

// カプセルの半径プロファイル。yは軸方向(中心が0)、Length=0で球
float Profile(float y, float L, float R)
{
	float d = max(abs(y) - L * 0.5, 0.0);
	return sqrt(max(R * R - d * d, 0.0));
}

//================================
// 頂点シェーダ：回転体シェルの形(半径・ねじれ・ノイズ)を全て頂点で作る
//================================
WindVSOut main(VSInput In)
{
	float v = In.UV.y;
	float halfLen = g_Length * 0.5 + g_Radius;
	float y = lerp(-halfLen, halfLen, v);

	float r = Profile(y, g_Length, g_Radius);
	r *= lerp(1.0 - g_Taper, 1.0, v);	// 後方ほどすぼめる

	// ねじれ+回転
	float a = In.UV.x * 6.2831853 + g_Twist * v + (g_Time + g_Seed) * g_SpinSpeed;

	// 半径をパーリンノイズで揺らす(両端はr=0なので動かない)
	float3 np = float3(cos(a), sin(a), y) * g_NoiseFreq + float3(g_Seed, g_Seed * 0.7, g_Seed * 1.3);
	r *= max(1.0 + PerlinNoise3D(np) * g_NoiseAmp, 0.0);

	float3 p = float3(r * cos(a), y, r * sin(a));
	float3 c = float3(0.0, clamp(y, -g_Length * 0.5, g_Length * 0.5), 0.0);	// 軸上の最近点

	WindVSOut o;
	o.wPos = mul(float4(p, 1.0), g_ShellWorld).xyz;
	o.wN = mul(normalize(p - c + float3(0.0, 1e-5, 0.0)), (float3x3)g_ShellWorld);

	o.Pos = mul(float4(o.wPos, 1.0), g_mView);
	o.Pos = mul(o.Pos, g_mProj);

	o.StreakUV = float2(In.UV.x * g_StreakCount + g_Seed,
	                    v * g_StreakStretch - (g_Time + g_Seed) * g_FlowSpeed);
	o.ShellUV = In.UV;
	o.Fade = saturate(v * 5.0) * saturate((1.0 - v) * 5.0);	// 両端をフェード
	return o;
}
