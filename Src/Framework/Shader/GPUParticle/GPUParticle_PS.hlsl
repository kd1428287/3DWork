#include "inc_GPUParticleShader.hlsli"
#include "inc_GPUParticleNoise.hlsli"

// パーティクル用テクスチャ
Texture2D g_tex : register(t1);

SamplerState g_ss : register(s0);

//================================
// ピクセルシェーダ
//================================
float4 main(VSOutput In) : SV_Target0
{
	float4 texColor = g_tex.Sample(g_ss, In.UV);

	float4 outColor = texColor * In.Color;

	// ディゾルブ有効時は線形フェードの代わりにノイズで消す
	if (!(g_NoiseFlags & NOISE_DISSOLVE))
	{
		// 寿命の終わりに向かってフェードアウトさせる
		outColor.a *= In.LifeRate;
	}

	return ApplyParticleNoise(outColor, In.UV, In.LifeRate, In.Seed);
}
