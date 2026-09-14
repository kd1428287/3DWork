#include "inc_KdGPUParticle.hlsli"

// パーティクル用テクスチャ
Texture2D g_tex : register(t1);

SamplerState g_ss : register(s0);

struct PSOutput
{
	float4 Color : SV_Target0;
	float Mask : SV_Target1; // 0=グレーディング適用, 1=完全除外
};


//================================
// ピクセルシェーダ
//================================

PSOutput main(VSOutput In)
{
	PSOutput Out;
	float4 texColor = g_tex.Sample(g_ss, In.UV);
	Out.Color = texColor * In.Color;
	Out.Color.a *= In.LifeRate;
	Out.Mask = Out.Color.a; // 不透明度に応じてマスクの強さも比例させる(半透明なら除外効果も弱める)
	return Out;
}
