#include "inc_KdGPUParticle.hlsli"

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

	// 寿命の終わりに向かってフェードアウトさせる
	outColor.a *= In.LifeRate;

	return outColor;
}
