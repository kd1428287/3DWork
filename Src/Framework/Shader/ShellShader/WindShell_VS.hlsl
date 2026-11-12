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

	// 基準角(時間を含まない)。cos/sinで座標化するので継ぎ目でノイズが連続する
	float a0 = In.UV.x * 6.2831853;
	float2 dir = float2(cos(a0), sin(a0));
	float tSeed = g_Time + g_Seed;

	// 回転：位置ごとに角度を揺らし、剛体回転にならないようにする(揺らぎは有界)
	float jitter = PerlinNoise3D(float3(dir * 1.3, y * 1.1) + float3(0.0, 0.0, tSeed * 0.4 + g_Seed));
	float a = a0 + g_Twist * v + tSeed * g_SpinSpeed + g_SpinJitter * jitter;

	// 半径の揺らぎ：回転に追従しつつ、時間でも流れる(両端はr=0なので動かない)
	float3 np = float3(cos(a), sin(a), y) * g_NoiseFreq
		+ float3(g_Seed, g_Seed * 0.7 + tSeed * g_NoiseSpeed, g_Seed * 1.3 + tSeed * g_NoiseSpeed * 0.7);
	r *= max(1.0 + PerlinNoise3D(np) * g_NoiseAmp, 0.0);

	float3 p = float3(r * cos(a), y, r * sin(a));
	float3 c = float3(0.0, clamp(y, -g_Length * 0.5, g_Length * 0.5), 0.0);	// 軸上の最近点

	WindVSOut o;
	o.wPos = mul(float4(p, 1.0), g_ShellWorld).xyz;
	o.wN = mul(normalize(p - c + float3(0.0, 1e-5, 0.0)), (float3x3)g_ShellWorld);

	o.Pos = mul(float4(o.wPos, 1.0), g_mView);
	o.Pos = mul(o.Pos, g_mProj);

	// 筋のUVをノイズで歪ませ、同じ模様の繰り返し・整列を崩す(時間でゆっくり変化)
	float3 wp = float3(dir * 1.1, v * 2.0) + float3(g_Seed * 0.31, g_Seed * 0.17, tSeed * 0.15);
	float2 warp = float2(PerlinNoise3D(wp), PerlinNoise3D(wp + float3(17.3, 5.1, 9.7)));

	o.StreakUV = float2(In.UV.x * g_StreakCount + g_Seed + warp.x * g_Scatter * 0.25,
	                    v * g_StreakStretch - tSeed * g_FlowSpeed + warp.y * g_Scatter * 0.5);
	o.ShellUV = In.UV;
	o.Fade = saturate(v * 5.0) * saturate((1.0 - v) * 5.0);	// 両端をフェード
	return o;
}
