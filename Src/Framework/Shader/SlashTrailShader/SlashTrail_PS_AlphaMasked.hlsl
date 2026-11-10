#include "../GPUParticle/inc_GPUParticleShader.hlsli"
#include "../GPUParticle/inc_GPUParticleNoise.hlsli"

// トレイル用テクスチャ
Texture2D g_tex : register(t1);

SamplerState g_ss : register(s0);

//================================
// ピクセルシェーダ出力
// SV_Target0 … 通常のカラー
// SV_Target1 … カラーグレード除外マスク(0=適用,1=除外)
//================================
struct PSOutput
{
	float4 Color : SV_Target0;
	float  GradeMask : SV_Target1;
};

//================================
// ピクセルシェーダ：トレイルの墨(Alpha)描画用
// ※GPUParticle_PS_Masked.hlslと同じ処理に、ノイズによる消去を足したもの
//   (パーティクル用のPSを変更せずに済むよう、トレイル専用として分離している)
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

	// 不透明度に応じてマスクの強さも比例させる
	Out.GradeMask = outColor.a;

	return Out;
}
