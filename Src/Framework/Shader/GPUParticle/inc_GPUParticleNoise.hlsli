// パーティクル用ノイズマスク/ディゾルブ(通常PSとMasked PSで共有)

cbuffer cbParticleNoise : register(b4)	// ※C++側kNoiseCBSlotと一致させる
{
	uint	g_NoiseFlags;
	float	g_NoiseScale;
	float2	g_NoiseScroll;

	float	g_MaskPower;
	float	g_DissolveStart;
	float	g_DissolveSoftness;
	float	g_EdgeWidth;

	float4	g_EdgeColor;
};

Texture2D g_noiseTex : register(t2);
SamplerState g_wrapSS : register(s1);

#define NOISE_MASK     (1u << 0)
#define NOISE_DISSOLVE (1u << 1)

float4 ApplyParticleNoise(float4 col, float2 uv, float lifeRate, float2 seed)
{
	if (g_NoiseFlags == 0) { return col; }

	float age01 = 1.0 - lifeRate;

	// 粒子ごとにオフセットして全粒子が同じ模様にならないようにする
	float2 nuv = uv * g_NoiseScale + seed + g_NoiseScroll * age01;
	float n = g_noiseTex.Sample(g_wrapSS, nuv).r;

	if (g_NoiseFlags & NOISE_MASK)
	{
		col.a *= pow(saturate(n), g_MaskPower);
	}

	if (g_NoiseFlags & NOISE_DISSOLVE)
	{
		float t = saturate((age01 - g_DissolveStart) / max(1.0 - g_DissolveStart, 1e-4));

		// t=0で発光帯がノイズ範囲外、t=1で全消失になるしきい値
		float band = g_DissolveSoftness + g_EdgeWidth;
		float th = t * (1.0 + band) - band;
		float d = n - th;	// d<0で消える

		col.a *= saturate(d / max(g_DissolveSoftness, 1e-4));

		// アルファが立ち上がりきった位置から発光させる(境界でalphaが0になるため)
		float edge = 1.0 - saturate((d - g_DissolveSoftness) / max(g_EdgeWidth, 1e-4));
		col.rgb += g_EdgeColor.rgb * g_EdgeColor.a * edge;
	}
	return col;
}
