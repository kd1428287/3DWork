// 水墨画化。t0=カラーグレード後の画像 t1=シーン深度

#include "../inc_KdCommon.hlsli"
#include "inc_KdPostProcessShader.hlsli"

Texture2D g_srcTex : register(t0);
Texture2D g_depthTex : register(t1);
SamplerState g_ss : register(s0);

// KdPostProcessShader::cbInkWashInfo と並びを一致させる
cbuffer cbInkWash : register(b0)
{
	float2 WashCenter;
	float WashProgress;
	float WashTime;

	float3 WashInkColor;
	float WashAspect;

	float3 WashPaperColor;
	float WashMaxDist;

	float WashSoftness;
	float WashContrast;
	float WashPivot;
	float WashBleed;

	float WashEdgeStrength;
	float WashDepthEdgeGain;
	float WashVignette;
	float WashPaperGrain;

	float WashPosterize;
	float WashNoiseScale;
	float2 _washPad;
};

float Hash21(float2 p)
{
	return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float VNoise(float2 p)
{
	float2 i = floor(p);
	float2 f = frac(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = Hash21(i);
	float b = Hash21(i + float2(1, 0));
	float c = Hash21(i + float2(0, 1));
	float d = Hash21(i + float2(1, 1));
	return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float FBM(float2 p)
{
	float v = 0.0;
	float a = 0.5;
	[unroll]
	for (int i = 0; i < 4; ++i)
	{
		v += a * VNoise(p);
		p = p * 2.03 + 17.1;
		a *= 0.5;
	}
	return v;
}

float Luma(float3 c)
{
	return dot(c, float3(0.2126, 0.7152, 0.0722));
}

float LoadLuma(int2 p, int2 dim)
{
	p = clamp(p, int2(0, 0), dim - int2(1, 1));
	return Luma(g_srcTex.Load(int3(p, 0)).rgb);
}

// ビュー空間のZ(DoFと同じ復元方法)
float ViewZ(int2 p, int2 dim)
{
	p = clamp(p, int2(0, 0), dim - int2(1, 1));
	float d = g_depthTex.Load(int3(p, 0)).r;
	float2 projXY = ((float2(p) + 0.5) / float2(dim) * 2.0 - 1.0) * float2(1, -1);
	float4 v = mul(float4(projXY, d, 1), g_mProjInv);
	return v.z / v.w;
}

float4 main(VSOutput In) : SV_Target0
{
	float4 src = g_srcTex.Sample(g_ss, In.UV);

	// 墨の広がり。縁をノイズで揺らす
	float2 aspectUV = In.UV * float2(WashAspect, 1.0);
	float2 c = (In.UV - WashCenter) * float2(WashAspect, 1.0);
	float d = length(c) / WashMaxDist + (FBM(aspectUV * 5.0 + WashTime * 0.03) - 0.5) * 0.25;
	float front = lerp(-0.2, 1.2 + WashSoftness, WashProgress);
	float cover = saturate((front - d) / max(WashSoftness, 1e-4));
	if (cover <= 0.0)
	{
		return src;
	}

	uint w, h;
	g_srcTex.GetDimensions(w, h);
	int2 dim = int2(w, h);
	int2 p = int2(In.Pos.xy);

	// 明暗を紙と墨の濃淡へ。ノイズでにじませ、数段の淡墨に丸める
	float n1 = FBM(aspectUV * WashNoiseScale + WashTime * 0.02);
	float tone = saturate((Luma(src.rgb) - WashPivot) * WashContrast + 0.5);
	tone = saturate(tone + (n1 - 0.5) * WashBleed);
	float x = tone * 3.0;
	float f = floor(x);
	float stepped = (f + smoothstep(0.35, 0.65, x - f)) / 3.0;
	tone = lerp(tone, stepped, WashPosterize);

	// 輪郭線。輝度のSobelと深度の段差を合わせ、ノイズでかすれさせる
	float l00 = LoadLuma(p + int2(-1, -1), dim);
	float l10 = LoadLuma(p + int2(0, -1), dim);
	float l20 = LoadLuma(p + int2(1, -1), dim);
	float l01 = LoadLuma(p + int2(-1, 0), dim);
	float l21 = LoadLuma(p + int2(1, 0), dim);
	float l02 = LoadLuma(p + int2(-1, 1), dim);
	float l12 = LoadLuma(p + int2(0, 1), dim);
	float l22 = LoadLuma(p + int2(1, 1), dim);
	float gx = (l20 + 2.0 * l21 + l22) - (l00 + 2.0 * l01 + l02);
	float gy = (l02 + 2.0 * l12 + l22) - (l00 + 2.0 * l10 + l20);
	float edgeL = smoothstep(0.1, 0.5, sqrt(gx * gx + gy * gy));

	float z = ViewZ(p, dim);
	float dz = abs(ViewZ(p + int2(1, 0), dim) - z)
		+ abs(ViewZ(p + int2(-1, 0), dim) - z)
		+ abs(ViewZ(p + int2(0, 1), dim) - z)
		+ abs(ViewZ(p + int2(0, -1), dim) - z);
	float edgeD = smoothstep(0.1, 0.5, dz / max(z, 1e-3) * WashDepthEdgeGain);

	float edge = max(edgeL, edgeD);
	edge *= lerp(1.0, smoothstep(0.3, 0.6, FBM(aspectUV * 60.0)), 0.6);
	float wash = min(tone, 1.0 - edge * WashEdgeStrength);

	// 周辺を墨で暗くする
	float vig = smoothstep(0.4, 1.0, length((In.UV - 0.5) * float2(WashAspect, 1.0)));
	vig *= 0.6 + 0.8 * n1;
	wash = saturate(wash - vig * WashVignette);

	// 紙の粒状感
	float3 paper = WashPaperColor * (1.0 - WashPaperGrain * FBM(aspectUV * 150.0));
	float3 ink = lerp(WashInkColor, paper, wash);

	// 広がりの縁は墨が溜まって濃くなる
	float3 rgb = lerp(src.rgb, ink, cover);
	rgb = lerp(rgb, WashInkColor, cover * (1.0 - cover) * 2.0);

	return float4(rgb, src.a);
}
