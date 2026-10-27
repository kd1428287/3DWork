// ※include・VSOutput構造体・テクスチャ/サンプラー宣言は PS_InkWash.hlsl の冒頭と同じものに置き換えること
// (t0=画像 t1=深度 s0=Linear_Clamp。下記の名前は仮置き)
#include "inc_KdPostProcessShader.hlsli"

Texture2D g_tex      : register(t0);
Texture2D g_depthTex : register(t1);
SamplerState g_ss    : register(s0);

// C++側のcbMotionBlurInfoと並びを一致させること
cbuffer cbMotionBlur : register(b0)
{
	row_major float4x4 g_reproject;	// 現在クリップ→前フレームクリップ
	float g_strength;				// シャッター係数
	int   g_sampleNum;
	float g_maxBlurUV;				// ブラー長の上限(画面UV比)
	float _pad;
};

float4 main(VSOutput In) : SV_Target0
{
	float2 uv = In.UV;
	float4 src = g_tex.SampleLevel(g_ss, uv, 0);
	float depth = g_depthTex.SampleLevel(g_ss, uv, 0).r;

	// 現在のクリップ座標を前フレームへ再投影
	float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
	float4 prev = mul(float4(ndc, depth, 1.0), g_reproject);
	if (prev.w <= 0.0) { return src; }

	float2 prevNdc = prev.xy / prev.w;
	float2 prevUV = float2(prevNdc.x * 0.5 + 0.5, 0.5 - prevNdc.y * 0.5);

	// 移動量(UV)。長すぎる場合は上限で打ち切る
	float2 vel = (uv - prevUV) * g_strength;
	float len = length(vel);
	if (len > g_maxBlurUV) { vel *= g_maxBlurUV / len; }

	// 現在位置を中心に速度方向へ前後サンプリング
	float3 sum = 0;
	[loop]
	for (int i = 0; i < g_sampleNum; ++i)
	{
		float t = i / (float)(g_sampleNum - 1) - 0.5;
		sum += g_tex.SampleLevel(g_ss, uv + vel * t, 0).rgb;
	}

	return float4(sum / g_sampleNum, src.a);
}
