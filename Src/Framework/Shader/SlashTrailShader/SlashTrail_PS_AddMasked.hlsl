#include "../GPUParticle/inc_GPUParticleShader.hlsli"
#include "../GPUParticle/inc_GPUParticleNoise.hlsli"

// トレイル用テクスチャ
Texture2D g_tex : register(t1);

SamplerState g_ss : register(s0);

//================================
// ピクセルシェーダ出力
// SV_Target0 … 通常のカラー(Add)
// SV_Target1 … DoFブラー除外マスク(カラーグレード除外マスクと共用。0=通常,1=除外)
//================================
struct PSOutput
{
	float4 Color : SV_Target0;
	float GradeMask : SV_Target1;
};

//================================
// ピクセルシェーダ：トレイルの芯(Add)描画用
// ※KdGPUParticle_PS.hlslと処理内容はほぼ同じだが、芯の存在範囲を
//   DoFブラー除外マスクとして書き出す点だけが異なる
//================================
PSOutput main(VSOutput In)
{
	float4 texColor = g_tex.Sample(g_ss, In.UV);

	float4 outColor = texColor * In.Color;

	// ディゾルブ無効時は線形フェード、有効時はノイズで消す(Seedはノイズ用のUV)
	if (!(g_NoiseFlags & NOISE_DISSOLVE)) { outColor.a *= In.LifeRate; }
	outColor = ApplyParticleNoise(outColor, In.Seed, In.LifeRate, float2(0.0f, 0.0f));

	PSOutput Out;
	Out.Color = outColor;

	// 芯の強さに応じてマスクを立てる
	// (背景の深度に引きずられてブラーがかかるのを防ぐ為の除外マスク)
	Out.GradeMask = saturate(outColor.a);

	return Out;
}
