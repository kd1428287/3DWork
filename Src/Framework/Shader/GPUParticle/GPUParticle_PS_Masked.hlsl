#include "inc_GPUParticleShader.hlsli"

// パーティクル用テクスチャ
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
// ピクセルシェーダ
// ※Alphaブレンドで描画する場合にのみ使用する(Add用の
//   KdGPUParticle_PS.hlslとは別ファイルとして分離している。
//   理由：Addは背景に対する加算値のため、このマスクの
//   考え方(1ピクセルを丸ごとグレーディング適用/除外で
//   切り替える)がそのままでは成立しないため。Add系パーティクルの
//   グレーディング除外は、別RTへの加算描画+事後合成で対応する)
//================================
PSOutput main(VSOutput In)
{
	float4 texColor = g_tex.Sample(g_ss, In.UV);

	float4 outColor = texColor * In.Color;

	// 寿命の終わりに向かってフェードアウトさせる
	outColor.a *= In.LifeRate;

	PSOutput Out;
	Out.Color = outColor;

	// 不透明度に応じてマスクの強さも比例させる
	// (薄く透けているピクセルほど背景色の寄与が大きいので、
	//  グレーディング除外を弱めて背景と馴染ませる)
	Out.GradeMask = outColor.a;

	return Out;
}
